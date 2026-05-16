#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <immintrin.h>

#define TARGET_HOST "www.nsa.gov"
#define TARGET_PORT 80
#define BUFFER_SIZE 256
#define LOOP_DELAY_US 500000 // 0.5 seconds delay between attempts

// Safely invoke the hardware random number generator
int get_rdrand16(unsigned short *rand_val) {
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
    return __builtin_ia32_rdrand16_step(rand_val);
#else
    #error "RDRAND is only supported on x86/x64 architectures."
#endif
}

// Send data via UDP to the specified target
void transmit_udp(const char *data, size_t data_len) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) return;

    struct hostent *server = gethostbyname(TARGET_HOST);
    if (server == NULL) {
        close(sock);
        return;
    }

    struct sockaddr_in target_addr;
    memset(&target_addr, 0, sizeof(target_addr));
    target_addr.sin_family = AF_INET;
    memcpy(&target_addr.sin_addr.s_addr, server->h_addr_list[0], server->h_length);
    target_addr.sin_port = htons(TARGET_PORT);

    sendto(sock, data, data_len, 0, (struct sockaddr *)&target_addr, sizeof(target_addr));
    close(sock);
}

int main() {
    unsigned short target_line;
    char buffer[BUFFER_SIZE];

    printf("Starting continuous RDRAND16 index lookup and forwarding...\n");

    while (1) {
        // 1. Generate a new hardware random number
        if (!get_rdrand16(&target_line)) {
            fprintf(stderr, "Hardware entropy pool temporarily unavailable.\n");
            usleep(LOOP_DELAY_US);
            continue;
        }

        // 2. Open the file to check the current contents
        FILE *file = fopen("cwords.txt", "r");
        if (file == NULL) {
            perror("Error opening cwords.txt");
            sleep(5); // Wait longer if the file is missing
            continue;
        }

        unsigned int current_line = 0;
        int found = 0;

        // 3. Scan the file for the matching index
        while (fgets(buffer, sizeof(buffer), file) != NULL) {
            current_line++;
            if (current_line == target_line) {
                found = 1;
                break;
            }
        }
        fclose(file);

        // 4. If the random index was within range, transmit the word
        if (found) {
            // Remove trailing newline characters if present
            buffer[strcspn(buffer, "\r\n")] = 0;
            
            transmit_udp(buffer, strlen(buffer));
        } else {
            // Index was outside the total line count of the file
        }

        // Delay to regulate execution speed
    }

    return 0;
}
