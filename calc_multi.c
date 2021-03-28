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

typedef struct compute_info {
    uint64_t thread_id;
    uint64_t num_threads;
    uint64_t *progress_location;
    char *result_filename;
    pthread_spinlock_t *result_lock;
} compute_info_t;

typedef struct timer_info {
    uint64_t num_threads;
    uint64_t *progress_array;
    char *progress_filename;
} timer_info_t;


static int OUT_OF_MEMORY = 0;


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


void write_result(const char *result_filename, pthread_spinlock_t *lock,
        uint64_t result) {
    pthread_spin_lock(lock);
    FILE *outfile = fopen(result_filename, "a");
    fprintf(outfile, "16^%llu\n", result);
    fclose(outfile);
    pthread_spin_unlock(lock);
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


void multiply_loop(array_ll_t *head, uint64_t *digits, uint64_t scale_factor,
        uint64_t end, uint64_t *progress, char *result_filename,
        pthread_spinlock_t *lock) {
    int i, is_pow_of_2;
    array_ll_t *curr_arr;
    uint64_t curr_digit, curr_entry, mult, new_entry, new_digit, carry = 0;
    while (OUT_OF_MEMORY == 0 && *progress < end) {
        curr_arr = head;
        is_pow_of_2 = 0;
        curr_digit = 0;
        while (curr_digit < *digits) {
            curr_entry = curr_arr->array[ENTRYIND(curr_digit)];
            new_entry = 0;
            for (i = 0; i < NIBBLES; i++) {
                mult = (curr_entry & 0xf) * scale_factor;
                new_digit = (mult + carry) % 10;
                carry = (mult + carry) / 10;
                curr_entry >>= 4;
                if ((new_digit & 1) + ((new_digit >> 1) & 1) + \
                        ((new_digit >> 2) & 1) + ((new_digit >> 3) & 1) == 1) {
                    is_pow_of_2 = 1;
                }
                new_entry |= new_digit << (i * 4);
                if (carry > 0 && (curr_digit + i) >= *digits - 1) {
                    *digits++;
                }
            }
            curr_arr->array[ENTRYIND(curr_digit)] = new_entry;
            curr_digit += NIBBLES;  // may well exceed digits, which is fine
            if (curr_digit % DIGITS == 0) {
                if (curr_arr->next == NULL) {
                    curr_arr->next = get_new_array();
                    if (curr_arr->next == NULL) {
                        OUT_OF_MEMORY = 1;
                        free_array_ll(head);
                        pthread_exit(NULL);
                    }
                    curr_arr = curr_arr->next;
                }
            }
        }
        *progress++;
        if (!is_pow_of_2) {
            write_result(result_filename, lock, *progress);
        }
        //printf("Printing %llu^%llu: Should be %llu digits\n", scale_factor, *progress, *digits);
        //print_number(head);
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
void *check_pow2_nibble(void *arg) {
    compute_info_t *info = (compute_info_t *)arg;
    *info->progress_location = 0;
    // store power of 16, rather than power of 2
    //int i, is_pow_of_2;
    uint64_t digits;
    //uint64_t curr_entry, mult, new_entry, new_digit, carry = 0;
    array_ll_t *curr_arr;
    array_ll_t *head = get_new_array();
    if (head == NULL) {
        OUT_OF_MEMORY = 1;
        pthread_exit(NULL);
    }
    head->array[0] = 0x1;
    digits = 1;
    multiply_loop(head, &digits, 16, info->thread_id, info->progress_location,
            info->result_filename, info->result_lock);
    multiply_loop(head, &digits, 16 << (4 * info->num_threads), ~0,
            info->progress_location, info->result_filename, info->result_lock);
}


void *run_timer(void *arg) {
    char *progress_filename = (char *)arg;
    uint64_t i, min;
    timer_info_t *info = (timer_info_t *)arg;
    while (OUT_OF_MEMORY == 0) {
        min = ~0;
        for (i = 0; i < info->num_threads; i++) {
            min = (info->progress_array[i] < min) ? info->progress_array[i] : min;
        }
        printf("Checked up to 16^%llu\n", min);
        write_progress(info->progress_filename, min);
        sleep(10);
    }
    pthread_exit(NULL);
}


int main(int argc, char *argv[]) {
    assert(DIGITS % NIBBLES == 0);
    uint64_t num_cores = sysconf(_SC_NPROCESSORS_ONLN) / 2;
    printf("%lu cores available\n", num_cores * 2);
    if (argc > 1) {
        printf("First argument is: %s\n", argv[1]);
        num_cores = strtol(argv[1], NULL, 10);
    }
    num_cores = (num_cores > 15) ? 15 : num_cores;
    // 16^15 is (2^64)/16, which is the maximum value which a 64-bit machine
    // can multiply by a base-10 digit without overflowing 2^64
    assert(num_cores > 0);

    uint64_t *progress_array = malloc(sizeof(uint64_t) * num_cores);

    char *progress_filename = "progress.txt";
    timer_info_t timer_info = {num_cores, progress_array, progress_filename};
    pthread_t timer_thread;
    pthread_create(&timer_thread, NULL, run_timer, (void *)&timer_info);

    char *result_filename = "results.txt";
    compute_info_t *info_array = malloc(sizeof(compute_info_t) * num_cores);
    pthread_t *thread_array = malloc(sizeof(pthread_t) * num_cores);
    pthread_spinlock_t lock;
    pthread_spin_init(&lock, 0);
    uint64_t i = 0;
    for (i = 0; i < num_cores; i++) {
        info_array[i].thread_id = i;
        info_array[i].num_threads = num_cores;
        info_array[i].progress_location = progress_array + i;
        info_array[i].result_filename = result_filename;
        info_array[i].result_lock = &lock;
        pthread_create(thread_array + i, NULL, check_pow2_nibble, info_array + i);
    }
    pthread_join(timer_thread, NULL);
    for (i = 0; i < num_cores; i++) {
        pthread_join(thread_array[i], NULL);
    }
    free(thread_array);
    free(info_array);
    free(progress_array);
    pthread_exit(NULL);
}
