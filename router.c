#include "protocols.h"
#include "queue.h"
#include "lib.h"
#include <string.h>
#include <arpa/inet.h>

struct route_table_entry *rtable;
int rtable_len;

struct arp_table_entry *arp_table; //echivalentul lui MAC table din laboratorul 4
int arp_table_len;

int main(int argc, char *argv[])
{
	char buf[MAX_PACKET_LEN];
	char packet_with_icmp[MAX_PACKET_LEN];

	// Do not modify this line
	init(argv + 2, argc - 2);

	rtable = malloc(sizeof(struct route_table_entry) * 100000);
	DIE(rtable == NULL, "Problem at rtable's malloc");

	//rtable_len = read_rtable("arp_table.txt", rtable);
	rtable_len = read_rtable(argv[1], rtable);
	DIE(rtable_len <= 0, "The arp table is empty!!!");

	arp_table = malloc(sizeof(struct arp_table_entry) * 100000);
	DIE(arp_table == NULL, "Problem at arp_table's malloc");

	//arp_table_len = parse_arp_table("arp_table.txt", arp_table);
	arp_table_len = parse_arp_table("arp_table.txt", arp_table);
	DIE(arp_table_len <= 0, "The arp table is empty!!!!");


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

		if (ntohs(ethernet_header->ethr_type) != 0x0800) { //verifica daca pachetul este IP ca sa nu inghesui alt protocol in sablonul de IP
			continue;
		}
		//ICMP = Internet Control Message Protocol
		//ICMP nu transporta date utile, ci mesaje de control
		//Sunt ca niste papusi matrioshka: Ethernet -> IP -> ICMP
		//deci pentru ICMP am nevoie de inca un "sablon", adica acel icmp_hdr din protocols.h

		//trebuie verificat daca adresa IP destinatie din pachet este egala cu adresa IP a interfetei pe unde a intrat pachetul
		//interfata pe care a intrat pachetul este interface
		if (ip_header->dest_addr == inet_addr(get_interface_ip(interface))) {
			struct icmp_hdr *icmp_req = (struct icmp_hdr *)(buf + sizeof(struct ether_hdr) + sizeof(struct ip_hdr));
		
		if (ip_header->proto != 1 || icmp_req->mtype != 8) {
			continue;
		}

		struct ether_hdr *eth_reply = (struct ether_hdr *)packet_with_icmp;
		struct ip_hdr   *ip_reply  = (struct ip_hdr *)(packet_with_icmp + sizeof(struct ether_hdr));
		struct icmp_hdr *icmp_reply = (struct icmp_hdr *)(packet_with_icmp + sizeof(struct ether_hdr) + sizeof(struct ip_hdr));

		memcpy(eth_reply->ethr_dhost, ethernet_header->ethr_shost, 6);
		get_interface_mac(interface, eth_reply->ethr_shost);
		eth_reply->ethr_type = htons(0x0800);

		ip_reply->ver     = 4;
		ip_reply->ihl     = 5;
		ip_reply->tos     = 0;
		ip_reply->tot_len = ip_header->tot_len;
		ip_reply->id      = 4;
		ip_reply->frag    = 0;
		ip_reply->ttl     = 64;
		ip_reply->proto   = 1;
		ip_reply->source_addr = ip_header->dest_addr;
		ip_reply->dest_addr   = ip_header->source_addr;
		ip_reply->checksum    = 0;
		ip_reply->checksum    = htons(checksum((uint16_t *)ip_reply, sizeof(struct ip_hdr)));

		size_t icmp_len = len - sizeof(struct ether_hdr) - sizeof(struct ip_hdr);
		memcpy(icmp_reply, icmp_req, icmp_len);
		icmp_reply->mtype = 0;
		icmp_reply->mcode = 0;
		icmp_reply->check = 0;
		icmp_reply->check = htons(checksum((uint16_t *)icmp_reply, icmp_len));

		send_to_link(sizeof(struct ether_hdr) + sizeof(struct ip_hdr) + icmp_len, packet_with_icmp, interface);
			
		continue;
	}

		//1.2 Verifica checksum:
		uint16_t checksum_primit = ip_header->checksum;
		ip_header->checksum = 0;
		uint16_t suma_recalculata = htons(checksum((uint16_t *)ip_header, sizeof(struct ip_hdr)));
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

			send_to_link(sizeof(struct icmp_hdr)+ sizeof(struct ip_hdr) + sizeof(struct ether_hdr) + sizeof(struct ip_hdr) + 8, packet_with_icmp, interface);

			continue; //s-a plimbat prea mult, il aruncam ca face prostii si tine ocupat reteaua
		}
		//uint8_t old_ttl = ip_header->ttl; //pastram ttl-ul vechi pentru a reverifica checksum-ul
		ip_header->ttl--; //scadem ttl-ul (TOTAL TIME LIMITTTT)
		//ip_header->checksum = ~(~ip_header->checksum + ~((uint16_t)old_ttl) + (uint16_t)ip_header->ttl) - 1; //furat din laboratorul 4
		ip_header->checksum = 0;
		ip_header->checksum = htons(checksum((uint16_t *)ip_header, sizeof(struct ip_hdr)));

		//1.4 Cautare in tabela de rutare
		struct route_table_entry *best_route = NULL;

		for (int i = 0; i < rtable_len; i++) {
			/*if (rtable[i].prefix == (ip_header->dest_addr & rtable[i].mask)) {
				if (best_route == NULL || rtable[i].mask > best_route->mask) {
					best_route = &rtable[i];
				}
			}*/
			if (ntohl(rtable[i].prefix) == (ntohl(ip_header->dest_addr) & ntohl(rtable[i].mask))) {
				if (best_route == NULL || ntohl(rtable[i].mask) > ntohl(best_route->mask)) {
					best_route = &rtable[i];
				}
			}
		}
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

			send_to_link(sizeof(struct icmp_hdr)+ sizeof(struct ip_hdr) + sizeof(struct ether_hdr) + sizeof(struct ip_hdr) + 8, packet_with_icmp, interface);

			continue;
		}

		//1.5 Actualizare checksum:
		//l-am calculat deja cand am scazut ttl-ul ca m-am luat dupa laboratorul 4

		//1.6 Rescriere adrese L2:
		struct arp_table_entry *destination_arp = NULL;
		for (int i = 0; i < arp_table_len; i++) {
			if(arp_table[i].ip == (uint32_t)best_route->next_hop) {
				destination_arp = &arp_table[i];
			}
		}

		if (destination_arp == NULL) {
			continue;
		}
		
		memcpy(ethernet_header->ethr_dhost, destination_arp->mac, 6);
		get_interface_mac(best_route->interface, ethernet_header->ethr_shost);

		//1.7: Trimiterea noului pachet pe interfata corespunzatoare urmatorului hop
		send_to_link(len, buf, best_route->interface);
	}
}

