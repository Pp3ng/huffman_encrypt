#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>
#include <pthread.h>

#define MAX_CHAR 256
#define BUFFER_SIZE 8192

typedef struct HuffmanNode
{
    unsigned char ch;
    unsigned int freq;
    struct HuffmanNode *left, *right;
} HuffmanNode;

typedef struct
{
    char *codes[MAX_CHAR];
} HuffmanTree;

typedef struct
{
    unsigned char byte;
    int bit_count;
} BitBuffer;

typedef struct
{
    HuffmanNode **data;
    int size;
    int capacity;
} PriorityQueue;

PriorityQueue *createQueue(int capacity)
{
    PriorityQueue *queue = (PriorityQueue *)malloc(sizeof(PriorityQueue));
    queue->data = (HuffmanNode **)malloc(sizeof(HuffmanNode *) * capacity);
    queue->size = 0;
    queue->capacity = capacity;
    return queue;
}

void swapNodes(HuffmanNode **a, HuffmanNode **b)
{
    HuffmanNode *temp = *a;
    *a = *b;
    *b = temp;
}

void heapify(PriorityQueue *queue, int i)
{
    int smallest = i;
    int left = 2 * i + 1;
    int right = 2 * i + 2;

    if (left < queue->size && queue->data[left]->freq < queue->data[smallest]->freq)
    {
        smallest = left;
    }

    if (right < queue->size && queue->data[right]->freq < queue->data[smallest]->freq)
    {
        smallest = right;
    }

    if (smallest != i)
    {
        swapNodes(&queue->data[smallest], &queue->data[i]);
        heapify(queue, smallest);
    }
}

void insertQueue(PriorityQueue *queue, HuffmanNode *node)
{
    if (queue->size == queue->capacity)
    {
        fprintf(stderr, "Queue overflow\n");
        return;
    }

    queue->size++;
    int i = queue->size - 1;
    queue->data[i] = node;

    while (i != 0 && queue->data[(i - 1) / 2]->freq > queue->data[i]->freq)
    {
        swapNodes(&queue->data[i], &queue->data[(i - 1) / 2]);
        i = (i - 1) / 2;
    }
}

HuffmanNode *extractMin(PriorityQueue *queue)
{
    if (queue->size <= 0)
        return NULL;
    if (queue->size == 1)
    {
        queue->size--;
        return queue->data[0];
    }

    HuffmanNode *root = queue->data[0];
    queue->data[0] = queue->data[queue->size - 1];
    queue->size--;
    heapify(queue, 0);

    return root;
}

HuffmanNode *createNode(unsigned char ch, unsigned int freq)
{
    HuffmanNode *node = (HuffmanNode *)malloc(sizeof(HuffmanNode));
    node->ch = ch;
    node->freq = freq;
    node->left = node->right = NULL;
    return node;
}

void freeTree(HuffmanNode *root)
{
    if (!root)
        return;
    freeTree(root->left);
    freeTree(root->right);
    free(root);
}

void generateCodes(HuffmanNode *root, char *code, int depth, HuffmanTree *ht)
{
    if (!root)
        return;

    if (!root->left && !root->right)
    {
        code[depth] = '\0';
        ht->codes[root->ch] = strdup(code);
    }

    if (root->left)
    {
        code[depth] = '0';
        generateCodes(root->left, code, depth + 1, ht);
    }

    if (root->right)
    {
        code[depth] = '1';
        generateCodes(root->right, code, depth + 1, ht);
    }
}

HuffmanNode *buildHuffmanTree(unsigned int freq[])
{
    PriorityQueue *queue = createQueue(MAX_CHAR);

    for (int i = 0; i < MAX_CHAR; i++)
    {
        if (freq[i] > 0)
        {
            insertQueue(queue, createNode(i, freq[i]));
        }
    }

    while (queue->size > 1)
    {
        HuffmanNode *left = extractMin(queue);
        HuffmanNode *right = extractMin(queue);

        HuffmanNode *merged = createNode(0, left->freq + right->freq);
        merged->left = left;
        merged->right = right;

        insertQueue(queue, merged);
    }

    HuffmanNode *root = extractMin(queue);
    free(queue->data);
    free(queue);

    return root;
}

void writeBit(int fd, BitBuffer *buffer, int bit)
{
    buffer->byte = (buffer->byte << 1) | bit;
    buffer->bit_count++;

    if (buffer->bit_count == 8)
    {
        write(fd, &buffer->byte, sizeof(unsigned char));
        buffer->bit_count = 0;
        buffer->byte = 0;
    }
}

void flushBuffer(int fd, BitBuffer *buffer)
{
    if (buffer->bit_count > 0)
    {
        buffer->byte <<= (8 - buffer->bit_count);
        write(fd, &buffer->byte, sizeof(unsigned char));
    }
}

typedef struct
{
    unsigned char *data;
    off_t start;
    off_t end;
    unsigned int *freq;
} ThreadData;

void *countFrequency(void *arg)
{
    ThreadData *td = (ThreadData *)arg;
    for (off_t i = td->start; i < td->end; i++)
    {
        __sync_fetch_and_add(&td->freq[td->data[i]], 1);
    }
    return NULL;
}

void encrypt(const char *inputFile, const char *outputFile)
{
    int in_fd = open(inputFile, O_RDONLY);
    if (in_fd == -1)
    {
        perror("Failed to open input file");
        return;
    }

    off_t in_size = lseek(in_fd, 0, SEEK_END);
    lseek(in_fd, 0, SEEK_SET);

    unsigned char *in_data = mmap(NULL, in_size, PROT_READ, MAP_PRIVATE, in_fd, 0);
    if (in_data == MAP_FAILED)
    {
        perror("Failed to mmap input file");
        close(in_fd);
        return;
    }

    unsigned int freq[MAX_CHAR] = {0};
    int num_threads = 4;
    pthread_t threads[num_threads];
    ThreadData td[num_threads];

    off_t chunk_size = in_size / num_threads;
    for (int i = 0; i < num_threads; i++)
    {
        td[i].data = in_data;
        td[i].start = i * chunk_size;
        td[i].end = (i == num_threads - 1) ? in_size : (i + 1) * chunk_size;
        td[i].freq = freq;
        pthread_create(&threads[i], NULL, countFrequency, &td[i]);
    }

    for (int i = 0; i < num_threads; i++)
    {
        pthread_join(threads[i], NULL);
    }

    HuffmanNode *root = buildHuffmanTree(freq);
    HuffmanTree ht = {0};
    char code[MAX_CHAR];
    generateCodes(root, code, 0, &ht);

    int out_fd = open(outputFile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (out_fd == -1)
    {
        perror("Failed to open output file");
        freeTree(root);
        munmap(in_data, in_size);
        close(in_fd);
        return;
    }

    // Write original file name
    size_t len = strlen(inputFile);
    write(out_fd, &len, sizeof(size_t));
    write(out_fd, inputFile, len);

    // Write number of unique characters
    int uniqueCount = 0;
    for (int i = 0; i < MAX_CHAR; i++)
    {
        if (ht.codes[i])
            uniqueCount++;
    }
    write(out_fd, &uniqueCount, sizeof(int));

    for (int i = 0; i < MAX_CHAR; i++)
    {
        if (ht.codes[i])
        {
            unsigned char len = strlen(ht.codes[i]);
            write(out_fd, &i, sizeof(unsigned char));
            write(out_fd, &freq[i], sizeof(unsigned int));
            write(out_fd, &len, sizeof(unsigned char));
            write(out_fd, ht.codes[i], len);
        }
    }

    // Write encrypted data
    BitBuffer buffer = {0, 0};
    unsigned char out_buffer[BUFFER_SIZE];
    size_t out_index = 0;

    for (off_t i = 0; i < in_size; i++)
    {
        char *code = ht.codes[in_data[i]];
        for (int j = 0; code[j]; j++)
        {
            buffer.byte = (buffer.byte << 1) | (code[j] - '0');
            buffer.bit_count++;

            if (buffer.bit_count == 8)
            {
                out_buffer[out_index++] = buffer.byte;
                buffer.bit_count = 0;
                buffer.byte = 0;

                if (out_index == BUFFER_SIZE)
                {
                    write(out_fd, out_buffer, BUFFER_SIZE);
                    out_index = 0;
                }
            }
        }
    }

    if (buffer.bit_count > 0)
    {
        buffer.byte <<= (8 - buffer.bit_count);
        out_buffer[out_index++] = buffer.byte;
    }

    if (out_index > 0)
    {
        write(out_fd, out_buffer, out_index);
    }

    freeTree(root);
    munmap(in_data, in_size);
    close(in_fd);
    close(out_fd);

    for (int i = 0; i < MAX_CHAR; i++)
    {
        free(ht.codes[i]);
    }
}

HuffmanNode *buildHuffmanTreeFromCodes(HuffmanTree *ht)
{
    HuffmanNode *root = createNode(0, 0);
    for (int i = 0; i < MAX_CHAR; i++)
    {
        if (ht->codes[i])
        {
            HuffmanNode *current = root;
            for (int j = 0; ht->codes[i][j]; j++)
            {
                if (ht->codes[i][j] == '0')
                {
                    if (!current->left)
                        current->left = createNode(0, 0);
                    current = current->left;
                }
                else
                {
                    if (!current->right)
                        current->right = createNode(0, 0);
                    current = current->right;
                }
            }
            current->ch = i;
        }
    }
    return root;
}

void decrypt(const char *inputFile)
{
    if (strstr(inputFile, ".huff") == NULL)
    {
        fprintf(stderr, "Invalid file format. Expected .huff file\n");
        return;
    }

    int in_fd = open(inputFile, O_RDONLY);
    if (in_fd == -1)
    {
        perror("Failed to open input file");
        return;
    }

    off_t in_size = lseek(in_fd, 0, SEEK_END);
    lseek(in_fd, 0, SEEK_SET);

    unsigned char *in_data = mmap(NULL, in_size, PROT_READ, MAP_PRIVATE, in_fd, 0);
    if (in_data == MAP_FAILED)
    {
        perror("Failed to mmap input file");
        close(in_fd);
        return;
    }

    size_t name_len;
    size_t offset = 0;

    memcpy(&name_len, in_data + offset, sizeof(size_t));
    offset += sizeof(size_t);

    char original_file[name_len + 1];
    memcpy(original_file, in_data + offset, name_len);
    original_file[name_len] = '\0';
    offset += name_len;

    int uniqueCount;
    memcpy(&uniqueCount, in_data + offset, sizeof(int));
    offset += sizeof(int);

    HuffmanTree ht = {0};
    for (int i = 0; i < uniqueCount; i++)
    {
        unsigned char ch, len;
        unsigned int freq;
        memcpy(&ch, in_data + offset, sizeof(unsigned char));
        offset += sizeof(unsigned char);

        memcpy(&freq, in_data + offset, sizeof(unsigned int));
        offset += sizeof(unsigned int);

        memcpy(&len, in_data + offset, sizeof(unsigned char));
        offset += sizeof(unsigned char);

        ht.codes[ch] = (char *)malloc(len + 1);
        memcpy(ht.codes[ch], in_data + offset, len);
        ht.codes[ch][len] = '\0';
        offset += len;
    }

    HuffmanNode *root = buildHuffmanTreeFromCodes(&ht);

    int out_fd = open(original_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (out_fd == -1)
    {
        perror("Failed to open output file");
        freeTree(root);
        for (int i = 0; i < MAX_CHAR; i++)
        {
            free(ht.codes[i]);
        }
        close(in_fd);
        munmap(in_data, in_size);
        return;
    }

    HuffmanNode *current = root;
    for (off_t i = offset; i < in_size; i++)
    {
        for (int j = 0; j < 8; j++)
        {
            if (in_data[i] & (1 << (7 - j)))
            {
                current = current->right;
            }
            else
            {
                current = current->left;
            }

            if (!current->left && !current->right)
            {
                write(out_fd, &current->ch, sizeof(unsigned char));
                current = root;
            }
        }
    }

    freeTree(root);
    munmap(in_data, in_size);
    close(in_fd);
    close(out_fd);

    for (int i = 0; i < MAX_CHAR; i++)
    {
        free(ht.codes[i]);
    }
}

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        fprintf(stderr, "Usage: %s -e <input_file> for encryption\n", argv[0]);
        fprintf(stderr, "       %s -d <input_file.huff> for decryption\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "-e") == 0)
    {
        char outputFile[] = "encrypted.huff";
        encrypt(argv[2], outputFile);
    }
    else if (strcmp(argv[1], "-d") == 0)
    {
        decrypt(argv[2]);
    }
    else
    {
        fprintf(stderr, "Unknown option: %s\n", argv[1]);
        return 1;
    }

    return 0;
}