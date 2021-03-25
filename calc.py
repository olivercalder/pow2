def main(in_file, out_file):
    while True:
        str_current_position = find_position(in_file)
        int_current_position = int(str_current_position)
        if int_current_position % 1000 == 0:
            print(int_current_position)
        int_full_number = 2 ** int_current_position
        str_full_number = str(int_full_number)
        digits = {"1", "2", "4", "8"}
        if not any(char in digits for char in str_full_number):
            append_if_answer_found(out_file, int_full_number)
        increment_in_file(in_file, (int_current_position + 1))


def find_position(in_file):
    file_in = open(in_file, "r")
    current_position = file_in.read()
    file_in.close()
    return current_position


def append_if_answer_found(out_file, full_number):
    file_out = open(out_file, "a")
    file_out.write(str(full_number))
    file_out.write("\n")
    file_out.close()
    return


def increment_in_file(in_file, num_to_write):
    file_in = open(in_file, 'w')
    file_in.write(str(num_to_write))
    file_in.close()
    return


if __name__ == "__main__":
    main("in.txt", "out.txt")
