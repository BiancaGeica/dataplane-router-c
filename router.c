#include "protocols.h"
#include "queue.h"
#include "lib.h"
#include <string.h>
#include <arpa/inet.h>

struct route_table_entry *rtable; //"agenda" cu directiile pe care le poate lua pachetul
int rtable_len;

//struct arp_table_entry *arp_table; //echivalentul lui MAC table din laboratorul 4, aici se gasesc adresele ip si mac ale fiecarui host si ale celor doua routere
//int arp_table_len;

//Task 2: Trie pentru longest prefix match

typedef struct node {
	struct node *left;
	struct node *right;
	struct route_table_entry *flag; //la sda flagul asta "coloreaza" nodul atunci cand s-a gasit cuvantul, dar mie imi trebuie si interfata care se gaseste in structura asta
}*Trie, TrieNode;

Trie creareTrie(void) {
	Trie root = malloc(sizeof(struct node));
	DIE(root == NULL, "Eroare la crearea triei");

	root->left = NULL;
	root->right = NULL;
	root->flag = NULL;

	return root;
}

Trie inserare(Trie trie, struct route_table_entry *node) {
	Trie iter = trie;

	int i = 31; //are 32 de biti, deci verific fiecare bit in parte si constuiesc arboree
	
	while (i >= 0) {
		if (((ntohl(node->mask) >> i) & 1) == 0) { //mai trebuiau parantezele la primul operator, inainte de == pt ca == este prioritar fata de &
			iter->flag = node;
			break;
		}

		if (((ntohl(node->prefix) >> i) & 1) == 0) {
			if (iter->left == NULL) {
				iter->left = malloc(sizeof(struct node));
				DIE(iter->left == NULL, "Eroare la alocarea memoriei in trie");

				iter->left->left = NULL;
				iter->left->right = NULL;
				iter->left->flag = NULL;
			}

			iter = iter->left;
		} else {
			if (iter->right == NULL) {
				iter->right = malloc(sizeof(struct node));
				DIE(iter->right == NULL, "Eroare la alocarea memoriei in trie");

				iter->right->left = NULL;
				iter->right->right = NULL;
				iter->right->flag = NULL;
			}

			iter = iter->right;
		}
		i--;
	}

	return trie;
}

struct route_table_entry *search(Trie trie, uint32_t ip_dest) {
	Trie iter = trie;
	struct route_table_entry *answer = NULL;

	int i = 31;
	while (i >= 0) {
		if (iter->flag != NULL) {
			answer = iter->flag;
		}
		if (((ntohl(ip_dest) >> i) & 1) == 0) {
			if (iter->left == NULL) {
				break;
			}

			iter = iter->left;
		} else {
			if (iter->right == NULL) {
				break;
			}

			iter = iter->right;
		}

		i--;
	}

	return answer;
}

int main(int argc, char *argv[]) //argv este util sa stiu ce routing table folosesc
{
	char buf[MAX_PACKET_LEN]; //buffer in care extrag toate informatiile primite
	char packet_with_icmp[MAX_PACKET_LEN]; //buffer in care pun toate informatiile pe care le trimit

	// Do not modify this line
	init(argv + 2, argc - 2);

	rtable = malloc(sizeof(struct route_table_entry) * 100000);
	DIE(rtable == NULL, "Problem at rtable's malloc");

	//rtable_len = read_rtable("arp_table.txt", rtable);
	rtable_len = read_rtable(argv[1], rtable);
	DIE(rtable_len <= 0, "The arp table is empty!!!");

	//arp_table = malloc(sizeof(struct arp_table_entry) * 100000);
	//DIE(arp_table == NULL, "Problem at arp_table's malloc");
	//TRAAAASH, nu mai avem rtable din care sa citim, deci bye
	struct arp_table_entry *memorie_cache = (struct arp_table_entry *)malloc(sizeof(struct arp_table_entry) * 100);
	DIE(memorie_cache == NULL, "Eroare la alocarea memoriei pt tabela arp cache");


	//arp_table_len = parse_arp_table("arp_table.txt", arp_table);
	//arp_table_len = parse_arp_table("arp_table.txt", arp_table);
	//DIE(arp_table_len <= 0, "The arp table is empty!!!!");
	//automat nu mai am nici lungimea pentru tabela aia statica veche, logic
	int len_memorie_cache = 0;

	//acum, la ARP, am nevoie de coada de asteptare in care o sa intre pachetele:
	queue coada_asteptare_pachete = create_queue();
	
	Trie root = creareTrie(); //creare trie si popularea ei
	for (int i = 0; i < rtable_len; i++) {
		root = inserare(root, &rtable[i]);
	}

	while (1) {

		size_t interface;
		size_t len;

		//interface este doar un numar, un id care anunta pe unde au intrat informatiile respective
		interface = recv_from_any_link(buf, &len); //router-ul primeste doar niste octeti "fara sens" momentan,
													//octetii respectivi trebuie convertiti pentru a putea lucra cu ei (cast)
		DIE(interface < 0, "recv_from_any_links");

    // TODO: Implement the router forwarding logic

    /* Note that packets received are in network order,
		any header field which has more than 1 byte will need to be conerted to
		host order. For example, ntohs(eth_hdr->ether_type). The oposite is needed when
		sending a packet on the link, */

		//Ok...the homework starts here
		/* ************************** */
		//Task 1: Procesul de dirijare
		//1.1 Verifica daca el este destinatia:
		struct ether_hdr *ethernet_header = (struct ether_hdr *)buf; //octetii sunt incadrati pe sablonul de header ethernet
																	//pentru a evita numaratul octetilor fara sesns, acestia sunt adaugati in structura asta pentru a-i accesa usor
		struct ip_hdr *ip_header = (struct ip_hdr *)(buf + sizeof(struct ether_hdr)); //dupa antetul ethernet, urmeaza cel IP, deci sarim beste bitii pe care i-am pus deja in structura ethernet
																					//apoi adaugam totul in header-ul IP din aceleasi motive

		if (ntohs(ethernet_header->ethr_type) == 0x0800) { //verifica daca pachetul este IP ca sa nu inghesui alt protocol in sablonul de IP
			//printf("PROBLEMA! Pachetul nu este de tipul IP!!! \n");
			//continue;
			//ook, acum nu mai avem doar ipv4 si garbadge, deci mut in if toata logica de mai devreme

			//ICMP = Internet Control Message Protocol
			//ICMP nu transporta date utile, ci mesaje de control
			//Sunt ca niste papusi matrioshka: Ethernet -> IP -> ICMP
			//deci pentru ICMP am nevoie de inca un "sablon", adica acel icmp_hdr din protocols.h

			//trebuie verificat daca adresa IP destinatie din pachet este egala cu adresa IP a interfetei pe unde a intrat pachetul
			//daca sunt egale, inseamna ca pachetul voia sa ajunga aici, nu este in tranzit
			//interfata pe care a intrat pachetul este interface
			if (ip_header->dest_addr == inet_addr(get_interface_ip(interface))) { //asta este momentul in care routerul primeste ping
				struct icmp_hdr *icmp_echo_request = (struct icmp_hdr *)(buf + sizeof(struct ether_hdr) + sizeof(struct ip_hdr));
			
			if (ip_header->proto != 1 || icmp_echo_request->mtype != 8) { //daca nu este icmp sau daca nu este echo request
				printf("PROBLEMA! Pachetul nu este ICMP \n");
				continue;
			}

			struct ether_hdr *ethernet_echo_reply = (struct ether_hdr *)packet_with_icmp;
			struct ip_hdr   *ip_echo_reply  = (struct ip_hdr *)(packet_with_icmp + sizeof(struct ether_hdr));
			struct icmp_hdr *icmp_echo_reply = (struct icmp_hdr *)(packet_with_icmp + sizeof(struct ether_hdr) + sizeof(struct ip_hdr));

			//interschimb in headerul ethernet sursa si destinatia si ii spun ca tipul pachetului este IPv4
			memcpy(ethernet_echo_reply->ethr_dhost, ethernet_header->ethr_shost, 6); //pentru ca trebuie sa intoarcem un mesaj, destinatia va fi sursa de la care a venit mesajul initial
			get_interface_mac(interface, ethernet_echo_reply->ethr_shost);
			ethernet_echo_reply->ethr_type = htons(0x0800);

			ip_echo_reply->ver     = 4;
			ip_echo_reply->ihl     = 5;
			ip_echo_reply->tos     = 0;
			ip_echo_reply->tot_len = ip_header->tot_len;
			ip_echo_reply->id      = 4;
			ip_echo_reply->frag    = 0;
			ip_echo_reply->ttl     = 64;
			ip_echo_reply->proto   = 1;
			ip_echo_reply->source_addr = ip_header->dest_addr;
			ip_echo_reply->dest_addr   = ip_header->source_addr;
			ip_echo_reply->checksum    = 0;
			ip_echo_reply->checksum    = htons(checksum((uint16_t *)ip_echo_reply, sizeof(struct ip_hdr)));

			memcpy(icmp_echo_reply, icmp_echo_request, len - sizeof(struct ether_hdr) - sizeof(struct ip_hdr));
			icmp_echo_reply->mtype = 0;
			icmp_echo_reply->mcode = 0;
			icmp_echo_reply->check = 0;
			icmp_echo_reply->check = htons(checksum((uint16_t *)icmp_echo_reply, len - sizeof(struct ether_hdr) - sizeof(struct ip_hdr)));

			send_to_link(sizeof(struct ether_hdr) + sizeof(struct ip_hdr) + len - sizeof(struct ether_hdr) - sizeof(struct ip_hdr), packet_with_icmp, interface);
			
			continue;
		}

			//1.2 Verifica checksum:
			uint16_t checksum_primit = ip_header->checksum;
			ip_header->checksum = 0;

			//network order = big endian = de la stanga la dreapta (cum vin datele/se duc pe fir)
			//host order = little endian = de la dreapta la stanga
			uint16_t suma_recalculata = htons(checksum((uint16_t *)ip_header, sizeof(struct ip_hdr)));
			//am pus htons la suma recalculata pentru ca suma recalculata trebuie sa plece pe fir, dar eu o calculez normal si astfel este initial in hostorder
			if(suma_recalculata != checksum_primit) {
				continue; //daca nu se potriveste suma recalculata cu suma din pachet, inseamna ca e corupt, TRASH
			}
			ip_header->checksum = checksum_primit;

			//1.3 Verificare si actualizare TTL:
			if (ip_header->ttl <= 1) {
				//creare sablon gol care este asezat de la adresa 0 a memoriei din "galeata" mea (adica din packet_with_icmp)
				struct ether_hdr *ethernet_hdr_icmp_ttl = (struct ether_hdr *)packet_with_icmp;
				struct ip_hdr *ip_hdr_icmp_ttl = (struct ip_hdr *)(packet_with_icmp + sizeof(struct ether_hdr));
				struct icmp_hdr *icmp_hdr_ttl = (struct icmp_hdr *)(packet_with_icmp + sizeof(struct ip_hdr) + sizeof(struct ether_hdr));

				char *payload_garbadge_packet_ttl = packet_with_icmp + sizeof(struct ether_hdr) + sizeof(struct ip_hdr) + sizeof(struct icmp_hdr);

				//in antetul ethernet se folosesc DOAR adresele fizice (mac)
				//pun ca sursa adresa mea curenta
				//toate adresele mac au 48 de biti, un byte are 8 biti => 48/8 = 6 octeti => trebuie sa copiem 6 bytes
				get_interface_mac(interface, ethernet_hdr_icmp_ttl->ethr_shost);
				memcpy(ethernet_hdr_icmp_ttl->ethr_dhost, ethernet_header->ethr_shost, 6);
				ethernet_hdr_icmp_ttl->ethr_type = htons(0x0800); //de pe wikipedia, 0x0800 Internet Protocol version 4 (IPv4) 
				
				ip_hdr_icmp_ttl->tos = 0;
				ip_hdr_icmp_ttl->tot_len = htons(sizeof(struct ip_hdr) + sizeof(struct icmp_hdr) + sizeof(struct ip_hdr) + 8);
				ip_hdr_icmp_ttl->id = 4;
				ip_hdr_icmp_ttl->frag = 0;
				ip_hdr_icmp_ttl->ttl = 64;
				ip_hdr_icmp_ttl->proto = 1;

				//ip_hdr_icmp_ttl->checksum = 0;
				//ip_hdr_icmp_ttl->checksum = htons(checksum((uint16_t *)ip_hdr_icmp_ttl, sizeof(struct ip_hdr)));
				//checksum trebuie calculat la final, dupa ce se pun adresele

				ip_hdr_icmp_ttl->source_addr = inet_addr(get_interface_ip(interface));
				ip_hdr_icmp_ttl->dest_addr = ip_header->source_addr;

				ip_hdr_icmp_ttl->checksum = 0;
				ip_hdr_icmp_ttl->checksum = htons(checksum((uint16_t *)ip_hdr_icmp_ttl, sizeof(struct ip_hdr)));

				icmp_hdr_ttl->mcode = 0;
				icmp_hdr_ttl->mtype = 11;
				
				//id si seq sunt folosite pentru ping, deci in cazul asta le setam la 0
				icmp_hdr_ttl->un_t.echo_t.id = 0;
				icmp_hdr_ttl->un_t.echo_t.seq = 0;

				memcpy(payload_garbadge_packet_ttl, ip_header, sizeof(struct ip_hdr) + 8);

				icmp_hdr_ttl->check = 0;
				icmp_hdr_ttl->check = htons(checksum((uint16_t *)icmp_hdr_ttl, sizeof(struct icmp_hdr) + sizeof(struct ip_hdr) + 8));

				printf("PROBLEMA! Pachetul a mers prea mult prin retea, TTL EXPIRAT \n");
				send_to_link(sizeof(struct icmp_hdr)+ sizeof(struct ip_hdr) + sizeof(struct ether_hdr) + sizeof(struct ip_hdr) + 8, packet_with_icmp, interface);

				continue; //s-a plimbat prea mult, il aruncam ca face prostii si tine ocupat reteaua
			}
			//uint8_t old_ttl = ip_header->ttl; //pastram ttl-ul vechi pentru a reverifica checksum-ul
			ip_header->ttl--; //scadem ttl-ul (TOTAL TIME LIMITTTT)
			//ip_header->checksum = ~(~ip_header->checksum + ~((uint16_t)old_ttl) + (uint16_t)ip_header->ttl) - 1; //furat din laboratorul 4
			ip_header->checksum = 0;
			ip_header->checksum = htons(checksum((uint16_t *)ip_header, sizeof(struct ip_hdr)));

			//1.4 Cautare in tabela de rutare
			/*struct route_table_entry *best_route = NULL;

			for (int i = 0; i < rtable_len; i++) {
				if (rtable[i].prefix == (ip_header->dest_addr & rtable[i].mask)) {
					if (best_route == NULL || rtable[i].mask > best_route->mask) {
						best_route = &rtable[i];
					}
				}
				if (ntohl(rtable[i].prefix) == (ntohl(ip_header->dest_addr) & ntohl(rtable[i].mask))) {
					if (best_route == NULL || ntohl(rtable[i].mask) > ntohl(best_route->mask)) {
						best_route = &rtable[i];
					}
				}
			}*/

			//Task 2: Longest prefix match eficient:
			struct route_table_entry *best_route = search(root, ip_header->dest_addr);

			if (best_route == NULL) {
				struct ether_hdr *ethernet_hdr_icmp_table = (struct ether_hdr *)packet_with_icmp;
				struct ip_hdr *ip_hdr_icmp_table = (struct ip_hdr *)(packet_with_icmp + sizeof(struct ether_hdr));
				struct icmp_hdr *icmp_hdr_table = (struct icmp_hdr *)(packet_with_icmp + sizeof(struct ip_hdr) + sizeof(struct ether_hdr));

				char *payload_garbadge_packet_table = packet_with_icmp + sizeof(struct ether_hdr) + sizeof(struct ip_hdr) + sizeof(struct icmp_hdr);

				get_interface_mac(interface, ethernet_hdr_icmp_table->ethr_shost);
				memcpy(ethernet_hdr_icmp_table->ethr_dhost, ethernet_header->ethr_shost, 6);
				ethernet_hdr_icmp_table->ethr_type = htons(0x0800);

				ip_hdr_icmp_table->tos = 0;
				ip_hdr_icmp_table->tot_len = htons(sizeof(struct ip_hdr) + sizeof(struct icmp_hdr) + sizeof(struct ip_hdr) + 8);
				ip_hdr_icmp_table->id = 4;
				ip_hdr_icmp_table->frag = 0;
				ip_hdr_icmp_table->ttl = 64;
				ip_hdr_icmp_table->proto = 1;

				ip_hdr_icmp_table->source_addr = inet_addr(get_interface_ip(interface));
				ip_hdr_icmp_table->dest_addr = ip_header->source_addr;

				ip_hdr_icmp_table->checksum = 0;
				ip_hdr_icmp_table->checksum = htons(checksum((uint16_t *)ip_hdr_icmp_table, sizeof(struct ip_hdr)));

				icmp_hdr_table->mcode = 0;
				icmp_hdr_table->mtype = 3;
					
				icmp_hdr_table->un_t.echo_t.id = 0;
				icmp_hdr_table->un_t.echo_t.seq = 0;

				memcpy(payload_garbadge_packet_table, ip_header, sizeof(struct ip_hdr) + 8);

				icmp_hdr_table->check = 0;
				icmp_hdr_table->check = htons(checksum((uint16_t *)icmp_hdr_table, sizeof(struct icmp_hdr) + sizeof(struct ip_hdr) + 8));

				printf("PROBLEMA! Nu s-a putut gasi o ruta valida \n");
				send_to_link(sizeof(struct icmp_hdr)+ sizeof(struct ip_hdr) + sizeof(struct ether_hdr) + sizeof(struct ip_hdr) + 8, packet_with_icmp, interface);

				continue;
			}

			//1.5 Actualizare checksum:
			//l-am calculat deja cand am scazut ttl-ul ca m-am luat dupa laboratorul 4

			//1.6/3.1 Rescriere adrese L2:
			struct arp_table_entry *destination_arp = NULL;
			//for (int i = 0; i < arp_table_len; i++) {
			for (int i = 0; i < len_memorie_cache; i++) {
				if(memorie_cache[i].ip == (uint32_t)best_route->next_hop) {
					destination_arp = &memorie_cache[i];
					break;
				}
			}

			if (destination_arp == NULL) {
				//printf("PROBLEMA! Nu s-a gasit o adresa de destinatie \n");

				//3.2 Salvarea pachetului pentru mai tarziu:
				printf("Nu s-a gasit o adresa destinatie, adaugare in coada ARP-ului....");
				//queue_enq(coada_asteptare_pachete, buf); nu este ok sa il pun pe buf direct, buf este un pointer la zona aia de memorie unde se primesc ping-urile si mi s-ar rescrie la urmatorul, pierzand informatia utila
				//pentru a evita suprascrierea, copiez "ping-ul" curent in ceva variabila si o folosesc pe aceea
				char *copie_ping = (char *)malloc(MAX_PACKET_LEN);
				DIE(copie_ping == NULL, "Eroare la alocarea memoriei la copia buffer-ului");

				memcpy(copie_ping, buf, len);

				queue_enq(coada_asteptare_pachete, copie_ping);

				//3.3 Generare ARP request
				//antet ethernet + antet arp <3

				char cerere_arp_ethr_hdr[MAX_PACKET_LEN];
				struct ether_hdr *header_ethernet_arp = (struct ether_hdr *)cerere_arp_ethr_hdr;
				struct arp_hdr *tot_requestul = (struct arp_hdr *)(cerere_arp_ethr_hdr + sizeof(struct ether_hdr));

				get_interface_mac(best_route->interface, header_ethernet_arp->ethr_shost);
				header_ethernet_arp->ethr_type = htons(0x0806); //este ARP
				header_ethernet_arp->ethr_dhost[0] = 0xFF; //adresa mac de destinatie este cea de broadcast
				header_ethernet_arp->ethr_dhost[1] = 0xFF;
				header_ethernet_arp->ethr_dhost[2] = 0xFF;
				header_ethernet_arp->ethr_dhost[3] = 0xFF;
				header_ethernet_arp->ethr_dhost[4] = 0xFF;
				header_ethernet_arp->ethr_dhost[5] = 0xFF;
				
				tot_requestul->hw_len = 6;
				tot_requestul->hw_type = htons(1);
				tot_requestul->opcode = htons(1);
				tot_requestul->proto_len = 4;
				tot_requestul->proto_type = htons(0x0800); //adica IPv4
				//tot_requestul->shwa = best_route->interface;
				get_interface_mac(best_route->interface, tot_requestul->shwa); //cine intreaba
				tot_requestul->sprotoa = inet_addr(get_interface_ip(best_route->interface));//despre cine este vorba
				tot_requestul->thwa[0] = 0;
				tot_requestul->thwa[1] = 0;
				tot_requestul->thwa[2] = 0;
				tot_requestul->thwa[3] = 0;
				tot_requestul->thwa[4] = 0;
				tot_requestul->thwa[5] = 0;

				tot_requestul->tprotoa = best_route->next_hop;

				//se trimite toata nebunia asta de request
				send_to_link(sizeof(struct ether_hdr) + sizeof(struct arp_hdr), cerere_arp_ethr_hdr, best_route->interface);

				continue;
			}
			
			memcpy(ethernet_header->ethr_dhost, destination_arp->mac, 6);
			get_interface_mac(best_route->interface, ethernet_header->ethr_shost);

			//specific arp (pas 3.1), pachetul trebuie trimis mai departe
			//1.7: Trimiterea noului pachet pe interfata corespunzatoare urmatorului hop
			send_to_link(len, buf, best_route->interface);
			printf("VICTORIE! Pachetul IPv4 a mers mai departe!!! \n");

		} else if (ntohs(ethernet_header->ethr_type) == 0x0806){
			/* AICI AM ARP*/
			//ARP = Address Resolution Protocol
			//arp ma ajuta sa gasesc adresa fizica unde trebuie sa ajunga un pachet

			//3.4: Parsare ARP reply
			//trebuie definita "memoria cache" in program pentru ca nu am voie sa mai am tabela
			//Logica ARP trebuie imbinata cu logica IPv4 pt. ca IPv4 are nevoie de MAC

			struct arp_hdr *reply = (struct arp_hdr *)(buf + sizeof(struct ether_hdr));

			if (ntohs(reply->opcode) == 2) { //1 = cerere, 2 = raspuns
				printf("Am primit un reply");

				memorie_cache[len_memorie_cache].ip = reply->sprotoa;
				//shwa = sender hardware address
				memcpy(memorie_cache[len_memorie_cache].mac, reply->shwa, 6); //lungimea adreselor mac este de 6
                len_memorie_cache++;

				queue aux = create_queue();
				while(queue_empty(coada_asteptare_pachete) == 0) {
					char *pachet_curent = queue_deq(coada_asteptare_pachete);

					struct ether_hdr *ethernet_header_pachet = (struct ether_hdr *)pachet_curent;
					struct ip_hdr *ip_header_curent = (struct ip_hdr *)(pachet_curent + sizeof(struct ether_hdr));

					struct route_table_entry *best_route = search(root, ip_header_curent->dest_addr);

					if (best_route != NULL && best_route->next_hop == reply->sprotoa) {
						//adaug mac-ul primit
						memcpy(ethernet_header_pachet->ethr_dhost, reply->shwa, 6);
                        get_interface_mac(best_route->interface, ethernet_header_pachet->ethr_shost);

						size_t len_pachet_curent = sizeof(struct ether_hdr) + ntohs(ip_header_curent->tot_len);

						printf("VICTORIE! S-a trimis pachetul ARP");
						send_to_link(len_pachet_curent, pachet_curent, best_route->interface);

						free(pachet_curent);
				} else {
					//pun inapoi pachetul in coada pentru a-l reprocesa, nu are ip-ul asteptat
					queue_enq(aux, pachet_curent);
				}
			}

			//pachetele ramase se muta in coada principala
			while (queue_empty(aux) == 0) {
                queue_enq(coada_asteptare_pachete, queue_deq(aux));
            }
		} else if (ntohs(reply->opcode) == 1) { //1 = cerere
			//atunci cand primesc o cerere, vreau sa vad daca este pentru mine, deci trebuie sa verific daca este ip-ul meu
			if (reply->tprotoa == inet_addr(get_interface_ip(interface))) {
				printf("Am primit request pentru ARP!!!");

				//pentru a trimite inapoi pachetul, trebuie sa interschimb adresele cum o faceam si la ipv4...ca trebuie sa se intoarca de la mine la cine a trimis initial...logic
				memcpy(ethernet_header->ethr_dhost, ethernet_header->ethr_shost, 6);
                get_interface_mac(interface, ethernet_header->ethr_shost);
				reply->opcode = htons(2); //schimb din 1 in 2 pentru ca acum nu mai este un request (ca raspund la el), ci un reply

				memcpy(reply->thwa, reply->shwa, 6); //cine a intrat devine "persoana" care trebuie sa primeasca
                reply->tprotoa = reply->sprotoa;

				get_interface_mac(interface, reply->shwa); //eu devin din receptor, emitator
                reply->sprotoa = inet_addr(get_interface_ip(interface));

				send_to_link(len, buf, interface);
			}
		}

		continue;
	}
}
}

