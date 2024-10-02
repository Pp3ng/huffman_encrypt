# Huffman Compression

This project implements Huffman coding for file encryption and decryption. Huffman coding is a lossless data compression algorithm that assigns variable-length codes to input characters, with shorter codes assigned to more frequent characters. This technique ensures that no data is lost during the process, making it ideal for applications where data integrity is crucial.

## Features

- **Compression**: Compresses a given file using Huffman coding.
- **Decompression**: Decompresses a `encrypted.huff` file back to its original form.

## Files

- `huffman.c`: Contains the implementation of Huffman coding for compression and decompression.

## Usage

### Compilation

To compile the program, use the following command:

```sh
gcc -o huffman huffman.c
```

### Demonstration

To encrypt a file, use the following command:

```sh
./huffman -c <input_file>
```

This will generate an executable file named `encrypted.huff`.

To decrypt the encrypted file, use the following command:

```sh
./huffman -d encrypted.huff
```

This will generate the original file with the original name.
