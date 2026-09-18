#include "protocols.h"
#include "queue.h"
#include "lib.h"
#include <string.h>
#include <arpa/inet.h>

struct route_table_entry *rtable; // "address book" with the directions the packet can take
int rtable_len;

//struct arp_table_entry *arp_table; // equivalent to the MAC table from lab 4, here we find the ip and mac addresses of each host and the two routers
//int arp_table_len;

// Task 2: Trie for longest prefix match

typedef struct node {
    struct node *left;
    struct node *right;
    struct route_table_entry *flag; // in SDA this flag "colors" the node when the word is found, but I also need the interface found in this structure
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

    int i = 31; // it has 32 bits, so I check each bit individually and build the tree
    
    while (i >= 0) {
        if (((ntohl(node->mask) >> i) & 1) == 0) { // parentheses were missing at the first operator before == because == has higher precedence than &
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

int main(int argc, char *argv[]) // argv is useful to know which routing table to use
{
    char buf[MAX_PACKET_LEN]; // buffer to extract all incoming information
    char packet_with_icmp[MAX_PACKET_LEN]; // buffer to put all outgoing information

    // Do not modify this line
    init(argv + 2, argc - 2);

    rtable = malloc(sizeof(struct route_table_entry) * 100000);
    DIE(rtable == NULL, "Problem at rtable's malloc");

    //rtable_len = read_rtable("arp_table.txt", rtable);
    rtable_len = read_rtable(argv[1], rtable);
    DIE(rtable_len <= 0, "The arp table is empty!!!");

    //arp_table = malloc(sizeof(struct arp_table_entry) * 100000);
    //DIE(arp_table == NULL, "Problem at arp_table's malloc");
    // TRAAAAASH, we don't have the rtable to read from anymore, so bye
    struct arp_table_entry *memorie_cache = (struct arp_table_entry *)malloc(sizeof(struct arp_table_entry) * 100);
    DIE(memorie_cache == NULL, "Eroare la alocarea memoriei pt tabela arp cache");


    //arp_table_len = parse_arp_table("arp_table.txt", arp_table);
    //arp_table_len = parse_arp_table("arp_table.txt", arp_table);
    //DIE(arp_table_len <= 0, "The arp table is empty!!!!");
    // automatically I don't have the length for that old static table anymore, logically
    int len_memorie_cache = 0;

    // now, for ARP, I need the waiting queue where packets will enter:
    queue coada_asteptare_pachete = create_queue();
    
    Trie root = creareTrie(); // creating the trie and populating it
    for (int i = 0; i < rtable_len; i++) {
        root = inserare(root, &rtable[i]);
    }

    while (1) {

        size_t interface;
        size_t len;

        // interface is just a number, an id that indicates where the information entered
        interface = recv_from_any_link(buf, &len); // the router receives "meaningless" bytes for now,
                                                    // those bytes need to be converted to be worked with (cast)
        DIE(interface < 0, "recv_from_any_links");

    // TODO: Implement the router forwarding logic

    /* Note that packets received are in network order,
        any header field which has more than 1 byte will need to be conerted to
        host order. For example, ntohs(eth_hdr->ether_type). The oposite is needed when
        sending a packet on the link, */

        // Ok...the homework starts here
        /* ************************** */
        // Task 1: Routing process
        // 1.1 Check if it is the destination:
        struct ether_hdr *ethernet_header = (struct ether_hdr *)buf; // bytes are cast onto the ethernet header template
                                                                    // to avoid counting meaningless bytes, they are added to this structure for easy access
        struct ip_hdr *ip_header = (struct ip_hdr *)(buf + sizeof(struct ether_hdr)); // after the ethernet header comes the IP one, so we skip the bits already in the ethernet structure
                                                                                    // then we add everything into the IP header for the same reasons

        if (ntohs(ethernet_header->ethr_type) == 0x0800) { // check if the packet is IP so we don't squeeze another protocol into the IP template
            //printf("PROBLEMA! Pachetul nu este de tipul IP!!! \n");
            //continue;
            // okay, now we don't just have ipv4 and garbage, so I move all the previous logic inside the if block

            // ICMP = Internet Control Message Protocol
            // ICMP does not transport payload data, but control messages
            // They are like Matryoshka dolls: Ethernet -> IP -> ICMP
            // so for ICMP I need another "template", meaning that icmp_hdr from protocols.h

            // need to check if the destination IP address in the packet is equal to the IP address of the interface the packet came through
            // if equal, it means the packet intended to arrive here, it is not in transit
            // the interface the packet came through is `interface`
            if (ip_header->dest_addr == inet_addr(get_interface_ip(interface))) { // this is the moment when the router receives a ping
                struct icmp_hdr *icmp_echo_request = (struct icmp_hdr *)(buf + sizeof(struct ether_hdr) + sizeof(struct ip_hdr));
            
            if (ip_header->proto != 1 || icmp_echo_request->mtype != 8) { // if it is not icmp or if it is not echo request
                printf("PROBLEMA! Pachetul nu este ICMP \n");
                continue;
            }

            struct ether_hdr *ethernet_echo_reply = (struct ether_hdr *)packet_with_icmp;
            struct ip_hdr   *ip_echo_reply  = (struct ip_hdr *)(packet_with_icmp + sizeof(struct ether_hdr));
            struct icmp_hdr *icmp_echo_reply = (struct icmp_hdr *)(packet_with_icmp + sizeof(struct ether_hdr) + sizeof(struct ip_hdr));

            // swap source and destination in the ethernet header and tell it the packet type is IPv4
            memcpy(ethernet_echo_reply->ethr_dhost, ethernet_header->ethr_shost, 6); // since we must return a message, the destination will be the source from the initial message
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

            // 1.2 Check checksum:
            uint16_t checksum_primit = ip_header->checksum;
            ip_header->checksum = 0;

            // network order = big endian = left to right (how data arrives/goes on the wire)
            // host order = little endian = right to left
            uint16_t suma_recalculata = htons(checksum((uint16_t *)ip_header, sizeof(struct ip_hdr)));
            // I added htons to recalculated sum because the sum must go on the wire, but I calculate it normally so it is initially in host order
            if(suma_recalculata != checksum_primit) {
                continue; // if recalculated sum doesn't match packet sum, it means it's corrupt, TRASH
            }
            ip_header->checksum = checksum_primit;

            // 1.3 Check and update TTL:
            if (ip_header->ttl <= 1) {
                // create empty template placed from memory address 0 of my "bucket" (inside packet_with_icmp)
                struct ether_hdr *ethernet_hdr_icmp_ttl = (struct ether_hdr *)packet_with_icmp;
                struct ip_hdr *ip_hdr_icmp_ttl = (struct ip_hdr *)(packet_with_icmp + sizeof(struct ether_hdr));
                struct icmp_hdr *icmp_hdr_ttl = (struct icmp_hdr *)(packet_with_icmp + sizeof(struct ip_hdr) + sizeof(struct ether_hdr));

                char *payload_garbadge_packet_ttl = packet_with_icmp + sizeof(struct ether_hdr) + sizeof(struct ip_hdr) + sizeof(struct icmp_hdr);

                // in the ethernet header ONLY physical addresses (mac) are used
                // set current address as source
                // all mac addresses have 48 bits, 1 byte has 8 bits => 48/8 = 6 bytes => we must copy 6 bytes
                get_interface_mac(interface, ethernet_hdr_icmp_ttl->ethr_shost);
                memcpy(ethernet_hdr_icmp_ttl->ethr_dhost, ethernet_header->ethr_shost, 6);
                ethernet_hdr_icmp_ttl->ethr_type = htons(0x0800); // from wikipedia, 0x0800 Internet Protocol version 4 (IPv4) 
                
                ip_hdr_icmp_ttl->tos = 0;
                ip_hdr_icmp_ttl->tot_len = htons(sizeof(struct ip_hdr) + sizeof(struct icmp_hdr) + sizeof(struct ip_hdr) + 8);
                ip_hdr_icmp_ttl->id = 4;
                ip_hdr_icmp_ttl->frag = 0;
                ip_hdr_icmp_ttl->ttl = 64;
                ip_hdr_icmp_ttl->proto = 1;

                //ip_hdr_icmp_ttl->checksum = 0;
                //ip_hdr_icmp_ttl->checksum = htons(checksum((uint16_t *)ip_hdr_icmp_ttl, sizeof(struct ip_hdr)));
                // checksum must be calculated at the end, after addresses are set

                ip_hdr_icmp_ttl->source_addr = inet_addr(get_interface_ip(interface));
                ip_hdr_icmp_ttl->dest_addr = ip_header->source_addr;

                ip_hdr_icmp_ttl->checksum = 0;
                ip_hdr_icmp_ttl->checksum = htons(checksum((uint16_t *)ip_hdr_icmp_ttl, sizeof(struct ip_hdr)));

                icmp_hdr_ttl->mcode = 0;
                icmp_hdr_ttl->mtype = 11;
                
                // id and seq are used for ping, so in this case set them to 0
                icmp_hdr_ttl->un_t.echo_t.id = 0;
                icmp_hdr_ttl->un_t.echo_t.seq = 0;

                memcpy(payload_garbadge_packet_ttl, ip_header, sizeof(struct ip_hdr) + 8);

                icmp_hdr_ttl->check = 0;
                icmp_hdr_ttl->check = htons(checksum((uint16_t *)icmp_hdr_ttl, sizeof(struct icmp_hdr) + sizeof(struct ip_hdr) + 8));

                printf("PROBLEMA! Pachetul a mers prea mult prin retea, TTL EXPIRAT \n");
                send_to_link(sizeof(struct icmp_hdr)+ sizeof(struct ip_hdr) + sizeof(struct ether_hdr) + sizeof(struct ip_hdr) + 8, packet_with_icmp, interface);

                continue; // traveled too long, drop it so it doesn't mess around and keep the network busy
            }
            //uint8_t old_ttl = ip_header->ttl; // keep old ttl to recheck checksum
            ip_header->ttl--; // decrement ttl (TOTAL TIME LIMITTTT)
            //ip_header->checksum = ~(~ip_header->checksum + ~((uint16_t)old_ttl) + (uint16_t)ip_header->ttl) - 1; // stolen from lab 4
            ip_header->checksum = 0;
            ip_header->checksum = htons(checksum((uint16_t *)ip_header, sizeof(struct ip_hdr)));

            // 1.4 Routing table search
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

            // Task 2: Efficient longest prefix match:
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

            // 1.5 Update checksum:
            // already calculated when decrementing ttl following lab 4

            // 1.6/3.1 Rewrite L2 addresses:
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

                // 3.2 Saving packet for later:
                printf("Nu s-a gasit o adresa destinatie, adaugare in coada ARP-ului.... \n");
                // queue_enq(coada_asteptare_pachete, buf); not okay to put buf directly, buf is a pointer to the memory area where pings are received and it would be overwritten on the next cycle, losing useful info
                // to avoid overwriting, copy current "ping" to a variable and use that one
                char *copie_ping = (char *)malloc(MAX_PACKET_LEN);
                DIE(copie_ping == NULL, "Eroare la alocarea memoriei la copia buffer-ului");

                memcpy(copie_ping, buf, len);

                queue_enq(coada_asteptare_pachete, copie_ping);

                // 3.3 Generate ARP request
                // ethernet header + arp header <3

                char cerere_arp_ethr_hdr[MAX_PACKET_LEN];
                struct ether_hdr *header_ethernet_arp = (struct ether_hdr *)cerere_arp_ethr_hdr;
                struct arp_hdr *tot_requestul = (struct arp_hdr *)(cerere_arp_ethr_hdr + sizeof(struct ether_hdr));

                get_interface_mac(best_route->interface, header_ethernet_arp->ethr_shost);
                header_ethernet_arp->ethr_type = htons(0x0806); // it is ARP
                header_ethernet_arp->ethr_dhost[0] = 0xFF; // destination mac address is broadcast
                header_ethernet_arp->ethr_dhost[1] = 0xFF;
                header_ethernet_arp->ethr_dhost[2] = 0xFF;
                header_ethernet_arp->ethr_dhost[3] = 0xFF;
                header_ethernet_arp->ethr_dhost[4] = 0xFF;
                header_ethernet_arp->ethr_dhost[5] = 0xFF;
                
                tot_requestul->hw_len = 6;
                tot_requestul->hw_type = htons(1);
                tot_requestul->opcode = htons(1);
                tot_requestul->proto_len = 4;
                tot_requestul->proto_type = htons(0x0800); // meaning IPv4
                //tot_requestul->shwa = best_route->interface;
                get_interface_mac(best_route->interface, tot_requestul->shwa); // who is asking
                tot_requestul->sprotoa = inet_addr(get_interface_ip(best_route->interface));// who is it about
                tot_requestul->thwa[0] = 0;
                tot_requestul->thwa[1] = 0;
                tot_requestul->thwa[2] = 0;
                tot_requestul->thwa[3] = 0;
                tot_requestul->thwa[4] = 0;
                tot_requestul->thwa[5] = 0;

                tot_requestul->tprotoa = best_route->next_hop;

                // send all this request craziness
                send_to_link(sizeof(struct ether_hdr) + sizeof(struct arp_hdr), cerere_arp_ethr_hdr, best_route->interface);

                continue;
            }
            
            memcpy(ethernet_header->ethr_dhost, destination_arp->mac, 6);
            get_interface_mac(best_route->interface, ethernet_header->ethr_shost);

            // arp specific (step 3.1), packet must be forwarded
            // 1.7: Send new packet on the interface corresponding to next hop
            send_to_link(len, buf, best_route->interface);
            printf("VICTORIE! Pachetul IPv4 a mers mai departe!!! \n");

        } else if (ntohs(ethernet_header->ethr_type) == 0x0806){
            /* HERE WE HAVE ARP */
            // ARP = Address Resolution Protocol
            // arp helps me find the physical address destination for a packet

            // 3.4: Parse ARP reply
            // "cache memory" must be defined in the program since we are no longer allowed to use a static table
            // ARP logic must be combined with IPv4 logic because IPv4 needs MAC addresses

            struct arp_hdr *reply = (struct arp_hdr *)(buf + sizeof(struct ether_hdr));

            if (ntohs(reply->opcode) == 2) { // 1 = request, 2 = reply
                printf("Am primit un reply \n");

                memorie_cache[len_memorie_cache].ip = reply->sprotoa;
                // shwa = sender hardware address
                memcpy(memorie_cache[len_memorie_cache].mac, reply->shwa, 6); // mac address length is 6
                len_memorie_cache++;

                queue aux = create_queue();
                while(queue_empty(coada_asteptare_pachete) == 0) {
                    char *pachet_curent = queue_deq(coada_asteptare_pachete);

                    struct ether_hdr *ethernet_header_pachet = (struct ether_hdr *)pachet_curent;
                    struct ip_hdr *ip_header_curent = (struct ip_hdr *)(pachet_curent + sizeof(struct ether_hdr));

                    struct route_table_entry *best_route = search(root, ip_header_curent->dest_addr);

                    if (best_route != NULL && best_route->next_hop == reply->sprotoa) {
                        // attach received mac
                        memcpy(ethernet_header_pachet->ethr_dhost, reply->shwa, 6);
                        get_interface_mac(best_route->interface, ethernet_header_pachet->ethr_shost);

                        size_t len_pachet_curent = sizeof(struct ether_hdr) + ntohs(ip_header_curent->tot_len);

                        printf("VICTORIE! S-a trimis pachetul ARP \n");
                        send_to_link(len_pachet_curent, pachet_curent, best_route->interface);

                        free(pachet_curent);
                } else {
                    // push packet back to queue to reprocess later, doesn't have expected IP
                    queue_enq(aux, pachet_curent);
                }
            }

            // remaining packets are moved back to main queue
            while (queue_empty(aux) == 0) {
                queue_enq(coada_asteptare_pachete, queue_deq(aux));
            }
        } else if (ntohs(reply->opcode) == 1) { // 1 = request
            // when receiving a request, check if it is for me, so check if it matches my IP
            if (reply->tprotoa == inet_addr(get_interface_ip(interface))) {
                printf("Am primit request pentru ARP!!! \n");

                // to send the packet back, swap addresses as done in ipv4... because it must return from me to initial sender... logic
                memcpy(ethernet_header->ethr_dhost, ethernet_header->ethr_shost, 6);
                get_interface_mac(interface, ethernet_header->ethr_shost);
                reply->opcode = htons(2); // change from 1 to 2 because now it's not a request anymore (replying to it), but a reply

                memcpy(reply->thwa, reply->shwa, 6); // sender becomes target
                reply->tprotoa = reply->sprotoa;

                get_interface_mac(interface, reply->shwa); // I change from receiver to sender
                reply->sprotoa = inet_addr(get_interface_ip(interface));

                send_to_link(len, buf, interface);
            }
        }

        continue;
    }
}
}