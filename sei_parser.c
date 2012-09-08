/**
 * Extracts the SEI information of a raw H.264 video stream
 * to check whether the frame_packing flag is set appropriately
 * for 3D stereoscopic video to be detected correctly by players
 * Compilable for Linux and Windows
 */


#define _CRT_SECURE_NO_WARNINGS // suppress windows compile warning for insecure fopen calls
#include <stdlib.h>
#include <stdio.h>

int mask[] = {0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01};

int readBit(unsigned char data[], int* bitpos) {
	int bit = (data[*bitpos/8] & mask[*bitpos%8]) != 0 ? 1 : 0;
	(*bitpos)++;
	return bit;
}

long readUnsigned(unsigned char data[], int* bitpos, int length) {
	long value = 0;
	while (length-->0)
		if (readBit(data, bitpos) == 1)
			value += 1<<length;
	return value;
}

long readUExpGolomb(unsigned char data[], int* bitpos) {
	int zeros = 0;
	while (readBit(data, bitpos) == 0)
		zeros++;
	return (1<<zeros)-1 + readUnsigned(data, bitpos, zeros);
}

void parse_sei_payload(unsigned char payload[], int payload_size) {
	int bitpos = 0;
	int frame_packing_arrangement_cancel_flag;
	long frame_packing_arrangement_type;
	int quincunx_sampling_flag;

	printf("frame_packing_arrangement_id = %li\n", readUExpGolomb(payload, &bitpos));
	frame_packing_arrangement_cancel_flag = readBit(payload, &bitpos);
	printf("frame_packing_arrangement_cancel_flag = %i\n", frame_packing_arrangement_cancel_flag);
	if (!frame_packing_arrangement_cancel_flag) {
		frame_packing_arrangement_type = readUnsigned(payload, &bitpos, 7);
		printf("frame_packing_arrangement_type = %li\n", frame_packing_arrangement_type);
		quincunx_sampling_flag = readBit(payload, &bitpos);
		printf("quincunx_sampling_flag = %i\n", quincunx_sampling_flag);
		printf("content_interpretation_type = %li\n", readUnsigned(payload, &bitpos, 6));
		printf("spatial_flipping_flag = %i\n", readBit(payload, &bitpos));
		printf("frame0_flipped_flag = %i\n", readBit(payload, &bitpos));
		printf("field_views_flag = %i\n", readBit(payload, &bitpos));
		printf("current_frame_is_frame0_flag = %i\n", readBit(payload, &bitpos));
		printf("frame0_self_contained_flag = %i\n", readBit(payload, &bitpos));
		printf("frame1_self_contained_flag = %i\n", readBit(payload, &bitpos));
		if (!quincunx_sampling_flag && frame_packing_arrangement_type != 5) {
			printf("frame0_grid_position_x = %li\n", readUnsigned(payload, &bitpos, 4));
			printf("frame0_grid_position_y = %li\n", readUnsigned(payload, &bitpos, 4));
			printf("frame1_grid_position_x = %li\n", readUnsigned(payload, &bitpos, 4));
			printf("frame1_grid_position_y = %li\n", readUnsigned(payload, &bitpos, 4));
		}
		printf("frame_packing_arrangement_reserved_byte = %li\n", readUnsigned(payload, &bitpos, 8));
		printf("frame_packing_arrangement_repetition_period = %li\n", readUExpGolomb(payload, &bitpos));
	}
	printf("frame_packing_arrangement_extension_flag = %i\n", readBit(payload, &bitpos));

	printf("rbsp trailer: ");
	while (bitpos < payload_size*8) {
		printf("%i", readBit(payload, &bitpos));
		if (bitpos % 8 == 0)
			printf(" ");
	}
	printf("\n");
}

void parse_sei_unit(FILE* f) {
	int payload_type = 0;
	int payload_size = 0;
	unsigned char* payload;
	int i, emu_count;

	printf("found a SEI NAL unit!\n");
	while (!feof(f) && payload_type % 0xFF == 0)
		payload_type += fgetc(f);
	printf("payload_type = %i\n", payload_type);
	while (!feof(f) && payload_size % 0xFF == 0)
		payload_size += fgetc(f);
	printf("payload_size = %i\n", payload_size);

	if (payload_type != 45)
		return;

	payload = (unsigned char*)malloc(payload_size*sizeof(unsigned char));
	for (i=0, emu_count=0; i<payload_size && !feof(f); i++, emu_count++) {
		payload[i] = fgetc(f);
		// drop emulation prevention bytes
		if (emu_count>=2 && payload[i] == 0x03 && payload[i-1] == 0x00 && payload[i-2] == 0x00) {
			i--;
			emu_count = 0;
		}
	}
	if (i != payload_size) {
		fprintf(stderr, "EOF reached while reading SEI payload");
		free(payload);
		return;
	}
	parse_sei_payload(payload, payload_size);
	free(payload);
}

void seek_sei(char* filename) {
	int offset = 0;
	int checklen = 0;
	FILE* f = fopen(filename, "r");
	while (!feof(f)) {
		if (checklen < 2 && fgetc(f) == 0x00)
			checklen++;
		else if (checklen == 2 && fgetc(f) == 0x01)
			checklen++;
		else if (checklen == 3 && fgetc(f) == 0x06) {
			parse_sei_unit(f);
			checklen = 0;
		}
		else
			checklen = 0;
	}
	fclose(f);
}

int main(int argc, char** args) {
	if (argc != 2)
		printf("Usage: sei_parser <h264-file>\n or simply drag&drop a h264-file on this program\n2011-11-03-schiermike\n\n");
	else
		seek_sei(args[1]);

	printf("Press any key to quit...\n");
	getchar();

	return 0;
}
