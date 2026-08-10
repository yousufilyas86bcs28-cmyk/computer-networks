#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define SEGMENT_SIZE 8

// Function prototypes
unsigned char compute_ones_complement_sum(const char *data, int num_segments);
void int_to_binary_str(unsigned char val, char *output);

int main() {
    char input_data[500];
    char padded_data[512] = {0};
    char checksum_str[SEGMENT_SIZE + 1];
    char transmitted_frame[600] = {0};
    char received_frame[600] = {0};
    int error_indices[10];
    int num_errors = 0;

    printf("=== Dynamic 8-bit Checksum Lab Simulation ===\n");
    printf("Enter continuous binary data stream: ");
    scanf("%499s", input_data);

    // Validate input data
    int input_len = strlen(input_data);

    for (int i = 0; i < input_len; i++) {
        if (input_data[i] != '0' && input_data[i] != '1') {
            printf("Error: Invalid binary input. Use only 0 and 1.\n");
            return 1;
        }
    }

    // 1. SENDER: Divide data into fixed-size segments
    strcpy(padded_data, input_data);

    int remainder = input_len % SEGMENT_SIZE;

    if (remainder != 0) {
        int padding_needed = SEGMENT_SIZE - remainder;

        for (int i = 0; i < padding_needed; i++) {
            strcat(padded_data, "0");
        }
    }

    int total_len = strlen(padded_data);
    int num_segments = total_len / SEGMENT_SIZE;

    printf("\n--- Sender Side ---\n");
    printf("Padded/Structured Data into %d segments:\n", num_segments);

    for (int i = 0; i < num_segments; i++) {
        printf("  Segment %d: ", i + 1);

        for (int j = 0; j < SEGMENT_SIZE; j++) {
            printf("%c", padded_data[i * SEGMENT_SIZE + j]);
        }

        printf("\n");
    }

    // Compute checksum
    unsigned char sum_val =
        compute_ones_complement_sum(padded_data, num_segments);

    unsigned char checksum_val =
        (unsigned char)(~sum_val & 0xFF);

    int_to_binary_str(checksum_val, checksum_str);

    // Construct transmitted frame
    sprintf(transmitted_frame, "%s%s",
            padded_data, checksum_str);

    printf("Calculated Checksum:  %s\n", checksum_str);
    printf("Transmitted Frame:    %s\n", transmitted_frame);

    // 2. CHANNEL SIMULATION
    strcpy(received_frame, transmitted_frame);

    int frame_len = strlen(received_frame);

    printf("\n--- Transmission Error Simulation ---\n");
    printf("How many bits do you want to corrupt/flip? (0 to %d): ",
           frame_len);

    scanf("%d", &num_errors);

    if (num_errors > 0) {
        printf("Enter the 0-indexed positions (from 0 to %d) to flip separated by spaces: ",
               frame_len - 1);

        for (int i = 0; i < num_errors; i++) {
            scanf("%d", &error_indices[i]);

            if (error_indices[i] >= 0 &&
                error_indices[i] < frame_len) {

                received_frame[error_indices[i]] =
                    (received_frame[error_indices[i]] == '0')
                    ? '1'
                    : '0';
            }
        }

        printf("[Channel] Successfully modified %d bit(s) inside the stream.\n",
               num_errors);
    } else {
        printf("[Channel] Transmitting frame perfectly with no modifications.\n");
    }

    // 3. RECEIVER
    printf("\n--- Receiver Side ---\n");
    printf("Received Frame:       %s\n", received_frame);

    // Include checksum segment
    int receiver_segments = num_segments + 1;

    unsigned char receiver_sum =
        compute_ones_complement_sum(received_frame,
                                     receiver_segments);

    unsigned char verification_result =
        (unsigned char)(~receiver_sum & 0xFF);

    printf("Receiver Final Sum:   ");

    char rec_sum_str[9];

    int_to_binary_str(receiver_sum, rec_sum_str);

    printf("%s\n", rec_sum_str);

    // Verify checksum
    if (verification_result == 0) {
        printf("Verification:         PASS\n");
        printf("Result:               Transmission is ERROR FREE! Data is intact.\n");

        // Extract original data
        char extracted_data[500] = {0};

        strncpy(extracted_data,
                received_frame,
                total_len);

        printf("Extracted Data:       %s\n", extracted_data);
    } else {
        printf("Verification:         FAIL\n");
        printf("Result:               Transmission is CORRUPTED! Errors detected.\n");
    }

    return 0;
}

// Computes the 1's complement summation of sequential 8-bit segments
unsigned char compute_ones_complement_sum(const char *data,
                                           int num_segments) {
    unsigned long sum = 0;
    char temp_seg[SEGMENT_SIZE + 1];

    for (int i = 0; i < num_segments; i++) {
        strncpy(temp_seg,
                data + (i * SEGMENT_SIZE),
                SEGMENT_SIZE);

        temp_seg[SEGMENT_SIZE] = '\0';

        // Convert binary string to numerical value
        unsigned long seg_val =
            strtoul(temp_seg, NULL, 2);

        sum += seg_val;

        // Wrap around carry bits
        while (sum >> 8) {
            sum = (sum & 0xFF) + (sum >> 8);
        }
    }

    return (unsigned char)(sum & 0xFF);
}

// Converts integer byte to 8-bit binary string
void int_to_binary_str(unsigned char val, char *output) {
    for (int i = 7; i >= 0; i--) {
        output[7 - i] =
            (val & (1 << i)) ? '1' : '0';
    }

    output[8] = '\0';
}
