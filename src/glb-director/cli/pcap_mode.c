#include "config.h"
#include "glb_director_config.h"
#include "glb_encap.h"
#include "glb_encap_pcap.h"
#include "glb_fwd_config.h"
#include "log.h"

#include <getopt.h>
#include <pcap.h>

#define ERRBUF_SIZE 1024

char config_file[256] = "";
char forwarding_table[256] = "";
char pcap_filename[256] = "";
char packet_filename[256] = "";

struct glb_processor_ctx *glb_lcore_contexts[RTE_MAX_LCORE] = {NULL};

int main(int argc, char **argv)
{
	int index, opt;

	static struct option long_options[] = {
	    {"config-file", required_argument, NULL, 'c'},
	    {"forwarding-table", required_argument, NULL, 't'},
	    {"pcap-file", required_argument, NULL, 'p'},
	    {"packet-file", required_argument, NULL, 'k'},
	    {NULL, 0, NULL, 0}};

	/* Find any command line options */

	while ((opt = getopt_long(argc, argv, ":c:t:p:", long_options, NULL)) !=
	       -1)
		switch (opt) {
		case 'c':
			strcpy(config_file, optarg);
			break;
		case 't':
			strcpy(forwarding_table, optarg);
			break;
		case 'p':
			strcpy(pcap_filename, optarg);
			break;
		case 'k':
			strcpy(packet_filename, optarg);
			break;
		case ':':
			/* missing option argument */
			glb_log_error("%s: option '-%c' requires an argument",
				      argv[0], optopt);
		case '?':
			/* invalid option */
			glb_log_error("Invalid option(s) in command");
			return 1;
		default:
			abort();
		}

	glb_log_info("Using config: %s, Using forwarding table: %s, Using pcap "
		     "file: %s, Using packet file: %s",
		     config_file, forwarding_table, pcap_filename,
		     packet_filename);

	for (index = optind; index < argc; index++)
		glb_log_error("Non-option argument %s", argv[index]);

	glb_log_info("Loading GLB configuration ...");
	g_director_config =
	    glb_director_config_load_file(config_file, forwarding_table);

	glb_log_info("Processing GLB config...");
	struct glb_fwd_config_ctx *config_ctx =
	    create_glb_fwd_config(g_director_config->forwarding_table_path);

	glb_log_info("GLB config context: %p", config_ctx);

	if (strlen(pcap_filename) > 0) {

		pcap_t *pcap;
		static char errbuf[ERRBUF_SIZE + 1];

		configuration conf[1] = {
		    {0, config_ctx} // use table 0
		};

		pcap = pcap_open_offline(pcap_filename, errbuf);

		if (pcap == NULL) {
			glb_log_error_and_exit("error: bad pcap file: %s",
					       pcap_filename);
		}

		pcap_loop(pcap, 0, (pcap_handler)glb_pcap_handler,
			  (u_char *)conf);

	} else if (strlen(packet_filename) > 0) {

		FILE *fp;
		unsigned char buffer[10240];
		int ret = 0;

		fp = fopen(packet_filename, "rb");

		ret = fread(buffer, sizeof(buffer), 1, fp);

		if (ret != 0) {
			glb_log_error_and_exit("failed to read packet file");
		}

		ret = glb_encapsulate_packet_pcap(config_ctx, buffer, 0);

		if (ret != 0) {
			glb_log_error_and_exit("packet encap failed!");
		}

		fclose(fp);
	}

	exit(0);
}