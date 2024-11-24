#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define ERROR 1
#define ABORTED 2
#define byte unsigned char

typedef struct
{
	bool iscompressed;
	bool istext; // text can't be compressed
	size_t height, width, compressed_size;
	byte* data;
} netpbm_image;

bool bytearray_getbit(byte* data, size_t i);
void bytearray_setbit(byte* data, size_t i, bool bit);
size_t calculate_size(size_t width, size_t height);
void readformat(char format1, char format2, bool* iscompressed, bool* istext);
void writeformat(bool iscompressed, bool istext, char* format1, char* format2);
netpbm_image readfile(char* input_file);
void writefile(netpbm_image image, char* output_file);
netpbm_image readtextfile(char* input_file);
void writetextfile(netpbm_image image, char* output_file);
void compress(char* input_file, char* output_file);
void decompress(char* input_file, char* output_file);
void texttobin(char* input_file, char* output_file);
void bintotext(char* input_file, char* output_file);

int main(int argc, char* argv[])
{
	if(argc != 4)
	{
		printf("Usage: %s (compress|decompress|texttobin|bintotext) <input_file> <output_file>\n", argv[0]);
		exit(ERROR);
	}
	if(strcmp(argv[1], "compress") == 0)
	{
		compress(argv[2], argv[3]);
	}
	else if(strcmp(argv[1], "decompress") == 0)
	{
		decompress(argv[2], argv[3]);
	}
	else if(strcmp(argv[1], "texttobin") == 0)
	{
		texttobin(argv[2], argv[3]);
	}
	else if(strcmp(argv[1], "bintotext") == 0)
	{
		bintotext(argv[2], argv[3]);
	}
	else
	{
		printf("Invalid argument \"%s\", use compress, decompress, texttobin or bintotext.\n", argv[1]);
		exit(ERROR);
	}
	printf("Conversion complete, output file is \"%s\".\n", argv[3]);
	return 0;
}

bool bytearray_getbit(byte* data, size_t i)
{
	size_t byte_index = i / 8;
	i = 7 - i % 8; // Reverse endiannes (so that bit 0 of 10000000 is 1)
	return (data[byte_index] >> i) & 1;
}

void bytearray_setbit(byte* data, size_t i, bool bit)
{
	size_t byte_index = i / 8;
	i = 7 - i % 8;
	if(bit) data[byte_index] |= 1 << i;
	else data[byte_index] &= ~(1 << i);
}

size_t calculate_size(size_t width, size_t height)
{
	size_t image_size = width * height;
	image_size = (image_size + 7) / 8;
	return image_size;
}

void readformat(char format1, char format2, bool* iscompressed, bool* istext)
{
	if(format1 == 'C')
	{
		*iscompressed = true;
		*istext = false;
	}
	else if(format1 == 'P')
	{
		*iscompressed = false;
		if(format2 == '1') *istext = true;
		else if(format2 == '4') *istext = false;
		else {printf("Invalid file format.\n"); exit(ERROR);}
	}
	else {printf("Invalid file format.\n"); exit(ERROR);}
}

void writeformat(bool iscompressed, bool istext, char* format1, char* format2)
{
	if(iscompressed)
	{
		*format1 = 'C';
		*format2 = '4';
	}
	else
	{
		*format1 = 'P';
		if(istext) *format2 = '1';
		else *format2 = '4';
	}
}

netpbm_image readfile(char* input_file)
{
	FILE *f = fopen(input_file, "rb");
	if(f == NULL)
	{
		printf("Error opening file %s.\n", input_file);
		exit(ERROR);
	}
	netpbm_image image;
	char format[2];
	fread(format, 1, 2, f);
	readformat(format[0], format[1], &image.iscompressed, &image.istext);
	fread(&image.width, sizeof(size_t), 1, f);
	fread(&image.height, sizeof(size_t), 1, f);
	if(image.iscompressed) fread(&image.compressed_size, sizeof(size_t), 1, f);
	size_t image_size = calculate_size(image.width, image.height);
	image.data = (byte*)malloc(image_size);
	fread(image.data, 1, image_size, f);
	fclose(f);
	return image;
}

void writefile(netpbm_image image, char* output_file)
{
	FILE *f = fopen(output_file, "wb");
	if(f == NULL)
	{
		printf("Error opening file %s.\n", output_file);
		exit(ERROR);
	}
	char format[2];
	writeformat(image.iscompressed, image.istext, &format[0], &format[1]);
	fwrite(format, 1, 2, f);
	fwrite(&image.width, sizeof(size_t), 1, f);
	fwrite(&image.height, sizeof(size_t), 1, f);
	if(image.iscompressed) fwrite(&image.compressed_size, sizeof(size_t), 1, f);
	size_t image_size = (image.iscompressed) ? image.compressed_size : calculate_size(image.width, image.height);
	fwrite(image.data, 1, image_size, f);
	fclose(f);
}

netpbm_image readtextfile(char* input_file)
{
	FILE *f = fopen(input_file, "r");
	if(f == NULL)
	{
		printf("Error opening file %s.\n", input_file);
		exit(ERROR);
	}
	netpbm_image image;
	char format[2];
	format[0] = fgetc(f);
	format[1] = fgetc(f);
	if(format[0] == 'C') {printf("Can't open compressed file as text.\n"); exit(ERROR);}
	if(format[1] == '4') {printf("Text file is actually binary.\n"); exit(ERROR);}
	readformat(format[0], format[1], &image.iscompressed, &image.istext);
	fscanf(f, "\n%zu %zu\n", &image.width, &image.height);
	size_t image_size = calculate_size(image.width, image.height), image_bits = image.width * image.height;
	image.data = (byte*)malloc(image_size);
	size_t bin_i = 0;
	for(size_t i = 0; i < image_bits; i++)
	{
		char current_char = fgetc(f);
		bool current_bit;
		if(current_char == '0') current_bit = 0;
		else if(current_char == '1') current_bit = 1;
		else continue;
		bytearray_setbit(image.data, bin_i++, current_bit);
		
	}
	fclose(f);
	return image;
}

void writetextfile(netpbm_image image, char* output_file)
{
	if(image.iscompressed) {printf("Can't write compressed file as text.\n"); exit(ERROR);}
	FILE *f = fopen(output_file, "w");
	if(f == NULL)
	{
		printf("Error opening file %s.\n", output_file);
		exit(ERROR);
	}
	char format[2];
	writeformat(image.iscompressed, image.istext, &format[0], &format[1]);
	fprintf(f, "%c%c\n%zu %zu", format[0], format[1], image.width, image.height);
	size_t image_bits = image.width * image.height;
	for(size_t i = 0; i < image_bits; i++)
	{
		if(i % image.width == 0) fputc('\n', f);
		bool current_bit = bytearray_getbit(image.data, i);
		char current_char = (current_bit) ? '1' : '0';
		fputc(current_char, f);
	}
	fputc('\n', f);
	fclose(f);
}

void compress(char* input_file, char* output_file)
{
	netpbm_image image = readfile(input_file);
	if(image.iscompressed == true || image.istext == true) {printf("Invalid file format.\n"); exit(ERROR);}
	size_t image_size = calculate_size(image.width, image.height);
	size_t image_bits = image.width * image.height;
	byte* compressed = (byte*)malloc(image_size);
	size_t compressed_i = 0;
	bool current_bit = bytearray_getbit(image.data, 0);
	byte repetitions = 1;
	for(size_t i = 1; i < image_bits; i++)
	{
		if(bytearray_getbit(image.data, i) != current_bit || repetitions > 127)
		{
			if(compressed_i >= image_size - 1)
			{
				printf("Compression aborted: original file size exceeded.\n");
				exit(ABORTED);
			}
			byte to_add = (current_bit << 7) | (repetitions - 1);
			compressed[compressed_i++] = to_add; // Format: 1 bit for value, 7 bits for repetitions (up to 128)
			printf("Compressed data: %d, Repetitions: %d\n", to_add, repetitions);
			current_bit = bytearray_getbit(image.data, i);
			repetitions = 1;
		}
		else repetitions++;
	}
	byte to_add = (current_bit << 7) | (repetitions - 1);
	compressed[compressed_i++] = to_add;
	printf("Compressed data: %d, Repetitions: %d\n", to_add, repetitions);
	free(image.data);
	image.data = compressed;
	image.iscompressed = true;
	image.compressed_size = compressed_i;
	writefile(image, output_file);
	free(image.data);
}

void decompress(char* input_file, char* output_file)
{
	netpbm_image image = readfile(input_file);
	if(image.iscompressed == false || image.istext == true) {printf("Invalid file format.\n"); exit(ERROR);}
	size_t image_size = calculate_size(image.width, image.height);
	byte* decompressed = (byte*)malloc(image_size);
	size_t decompressed_i = 0;
	for(size_t i = 0; i < image.compressed_size; i++)
	{
		byte current = image.data[i];
		bool bit = current >> 7;
		byte repetitions = (current & 0b01111111) + 1;
		printf("Decompressing: %d, Repetitions: %d\n", current, repetitions);
		for(int j = 0; j < repetitions; j++) bytearray_setbit(decompressed, decompressed_i++, bit);
	}
	free(image.data);
	image.data = decompressed;
	image.iscompressed = false;
	image.compressed_size = 0;
	writefile(image, output_file);
	free(image.data);
}

void texttobin(char* input_file, char* output_file)
{
	netpbm_image image = readtextfile(input_file);
	image.istext = false;
	writefile(image, output_file);
	free(image.data);
}

void bintotext(char* input_file, char* output_file)
{
	netpbm_image image = readfile(input_file);
	if(image.iscompressed == true || image.istext == true) {printf("Invalid file format.\n"); exit(ERROR);}
	image.istext = true;
	writetextfile(image, output_file);
	free(image.data);
}
