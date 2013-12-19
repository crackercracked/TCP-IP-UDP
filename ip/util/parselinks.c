#include <arpa/inet.h>
#include <inttypes.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "parselinks.h"
#include "list.h"

int parse_triple(FILE *f, char phys_host[HOST_MAX_LENGTH], uint16_t *phys_port,
                 struct in_addr *virt_ip)
{
    char phys_host_in[256];
    int phys_port_in;
    int virt_ip_in[4];
    int i;

    int ret = fscanf(f, "%255[^:]:%d %d.%d.%d.%d ", phys_host_in, &phys_port_in,
                     virt_ip_in, virt_ip_in+1, virt_ip_in+2, virt_ip_in+3);
    if (ret != 6) {
        return -1;
    }

    (void)strcpy(phys_host, phys_host_in);

    if (phys_port_in < 0x0000 || phys_port_in > 0xffff) {
        return -1;
    }
    *phys_port = phys_port_in;

    virt_ip->s_addr = 0;
    for (i=0; i<4; i++){
        if (virt_ip_in[i] < 0 || virt_ip_in[i] > 255) {
            return -1;
        }
        virt_ip->s_addr |= virt_ip_in[i] << (24-i*8);
    }
    return 0;
}

int parse_line(FILE *f, link_t *link)
{
    int ret = parse_triple(f, link->local_phys_host, &link->local_phys_port,
                           &link->local_virt_ip);
    if (ret == -1) {
        return -1;
    }
    
    ret = parse_triple(f, link->remote_phys_host, &link->remote_phys_port,
                       &link->remote_virt_ip);
    if (ret == -1) {
        return -1;
    }

    return 0;
}

int count_links(FILE *f)
{
    int c;
    int lines = 0;
    while ((c = fgetc(f)) != EOF){
        if (c == '\n') {
            lines++;
        }
    }
    return lines;
}

list_t *parse_links(char *filename)
{
    int ret;
    FILE *f;
    list_t *links;
    link_t *line;
    f = fopen(filename,"r");
  
    if (f == NULL){
        return NULL;
    }

    // Initialize the list of links.
    list_init(&links);

    while (!feof(f)) {
        line = (link_t *)malloc(sizeof(link_t));
        if (line == NULL) {
            fprintf(stderr, "parse_links: out of memory\n");
            exit(1);
        }
	
        ret = parse_line(f, line);
        if (ret == -1) {
            free(line);
            return links;
        }
        list_append(links, (void *)(line));
    }

    fclose(f);
    return links;
}

void free_links(list_t *links)
{
    node_t *curr;
	for (curr = links->head; curr != NULL; curr = curr->next) {
		free (curr->data);
	}
	list_free(&links);
}
