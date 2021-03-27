/* Written by Oliver Calder, March 2021
 *
 * This program checks powers of 2 to find those which do not have any digits
 * which are powers of 2 (namely, 1, 2, 4, and 8).
 *
 * This implementation uses nibbles to store 16 base-10 digits per uint64, and
 * stores those uint64s in arrays of 4096 bytes, keeping a linked list of
 * pointers to the beginning of each of these arrays. */


#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <assert.h>
#include <unistd.h>
#include <pthread.h>

#define ARRAYBYTES  4096                    // total bytes per array
#define DATASIZE    8                       // bytes per array entry

#define ENTRYIND(digit)    ((digit % DIGITS) / NIBBLES)

const uint64_t ARRAYSIZE = ARRAYBYTES / DATASIZE;   // entries per array
const uint64_t NIBBLES = DATASIZE * 2;              // nibbles per array entry
const uint64_t DIGITS = ARRAYBYTES * 2;             // digits (nibbles) per array

typedef struct linked_list {
    uint64_t *array;
    struct linked_list *next;
} array_ll_t;


static int OUT_OF_MEMORY = 0;
static uint64_t POWER_OF_16 = 0;


array_ll_t *get_new_array() {
    uint64_t *new_array = malloc(sizeof(uint64_t) * ARRAYSIZE);
    if (new_array == NULL) {
        return NULL;
    }
    for (int index = 0; index < ARRAYSIZE; index++) {
        new_array[index] = 0;
    }
    array_ll_t *new_ll_entry = malloc(sizeof(array_ll_t));
    if (new_ll_entry == NULL) {
        free(new_array);
        return NULL;
    }
    new_ll_entry->array = new_array;
    new_ll_entry->next = NULL;
    return new_ll_entry;
}


void free_array_ll(array_ll_t *head) {
    array_ll_t *next;
    while (head != NULL) {
        free(head->array);
        next = head->next;
        free(head);
        head = next;
    }
}


void write_progress(const char *progress_filename, uint64_t progress) {
    FILE *outfile = fopen(progress_filename, "w");
    fprintf(outfile, "%llu\n", progress);
    fclose(outfile);
}


void write_result(const char *result_filename, uint64_t result) {
    FILE *outfile = fopen(result_filename, "a");
    fprintf(outfile, "16^%llu\n", POWER_OF_16);
    fclose(outfile);
}


void print_number(array_ll_t *head) {
    // Prints in order within arrays, but prints arrays in reverse order
    array_ll_t *next;
    int pages = 0;
    while (head != NULL) {
        printf("Printing page %d:\n", pages);
        int hit_nonzero = 0;
        for (int i = ARRAYSIZE - 1; i >= 0; i--) {
            uint64_t curr_entry = head->array[i];
            for (int j = NIBBLES - 1; j >= 0; j--) {
                uint64_t digit = (curr_entry >> (4 * j)) & 0xf;
                if (digit != 0) {
                    hit_nonzero = 1;
                    printf("%llu", digit);
                } else if (hit_nonzero) {
                    printf("%llu", digit);
                }
            }
        }
        printf("\n");
        pages++;
        head = head->next;
    }
}


/* Checks powers of 2 for any which, when expressed in base 10, have no digits
 * which are themselves powers of 2.  Due to the default 64-bit integer limit
 * in C, and the trouble of computing a base 10 representation of a large power
 * of 2, instead chooses to store an array of uint64_t, each of which stores 16
 * base 10 numbers, one in each of the 16 4-bit nibbles.  For each iteration,
 * multiplies the base 10 number in the nibble by 16 (since 2^{1,2,3}n always
 * ends in {2,4,8} and thus can be immediately excluded), stores the result
 * mod 10 in that same nibble, and carries the result divided by 10 to the next
 * nibble, which is either in the same uint64_t or in the next. */
uint64_t check_pow2_nibble(const char *result_filename) {
    POWER_OF_16 = 0;
    // store power of 16, rather than power of 2
    int i, is_pow_of_2;
    uint64_t arrays = 1, digits = 1, curr_digit = 0;
    uint64_t curr_entry, mult, new_entry, new_digit, carry = 0;
    array_ll_t *curr_arr;
    array_ll_t *head = get_new_array();
    if (head == NULL) {
        OUT_OF_MEMORY = 1;
        printf("OUT OF MEMORY at 16^%llu");
        return POWER_OF_16;
    }
    array_ll_t *tail = head;
    head->array[0] = 0x1;
    while (1) {
        curr_arr = head;
        is_pow_of_2 = 0;
        curr_digit = 0;
        while (curr_digit < digits) {
            curr_entry = curr_arr->array[ENTRYIND(curr_digit)];
            new_entry = 0;
            for (i = 0; i < NIBBLES; i++) {
                mult = (curr_entry & 0xf) * 16;
                new_digit = (mult + carry) % 10;
                carry = (mult + carry) / 10;
                curr_entry >>= 4;
                if ((new_digit & 1) + ((new_digit >> 1) & 1) + \
                        ((new_digit >> 2) & 1) + ((new_digit >> 3) & 1) == 1) {
                    is_pow_of_2 = 1;
                }
                new_entry |= new_digit << (i * 4);
                if (carry > 0 && (curr_digit + i) >= digits - 1) {
                    digits++;
                }
            }
            curr_arr->array[ENTRYIND(curr_digit)] = new_entry;
            curr_digit += NIBBLES;  // may well exceed digits, which is fine
            if (curr_digit % DIGITS == 0) {
                curr_arr = curr_arr->next;
                if (curr_arr == NULL) {
                    tail->next = get_new_array();
                    if (tail->next == NULL) {
                        OUT_OF_MEMORY = 1;
                        printf("OUT_OF_MEMORY at 16^%llu", POWER_OF_16);
                        free_array_ll(head);
                        return POWER_OF_16;
                    }
                    tail = tail->next;
                    curr_arr = tail;
                }
            }
        }
        POWER_OF_16++;
        if (!is_pow_of_2) {
            write_result(result_filename, POWER_OF_16);
        }
        //printf("Printing 16^%llu: Should be %llu digits\n", POWER_OF_16, digits);
        //print_number(head);
    }
}


void *run_timer(void *arg) {
    const char *progress_filename = (const char *)arg;
    while (OUT_OF_MEMORY == 0) {
        printf("Checked up to 16^%llu\n", POWER_OF_16);
        write_progress(progress_filename, POWER_OF_16);
        sleep(10);
    }
    pthread_exit(NULL);
}


int main() {
    assert(DIGITS % NIBBLES == 0);
    pthread_t timer_thread;
    const char *progress_filename = "progress.txt";
    pthread_create(&timer_thread, NULL, run_timer, (void *)progress_filename);
    const char *results_filename = "results.txt";
    uint64_t max_power_of_16 = check_pow2_nibble(results_filename);
    pthread_join(timer_thread, NULL);
    pthread_exit(NULL);
}
