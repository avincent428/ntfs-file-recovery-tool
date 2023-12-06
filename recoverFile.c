#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#define BUFFER_SIZE 4096

// Structure to represent an MBR entry
struct MBRPartitionEntry {
    uint8_t status;
    uint8_t chs_start[3];
    uint8_t type;
    uint8_t chs_end[3];
    uint32_t lba_start;
    uint32_t num_sectors;
};

struct NTFSBootRecord {
    char jmp[3];
    char oem_id[8];
    uint16_t bytes_per_sector; // bytes per sector
    uint8_t sectors_per_cluster; // sectores per cluster
    uint16_t reserved_sectors;
    uint8_t zero1[3];
    uint16_t zero2;
    uint8_t media_descriptor;
    uint16_t zero3;
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t hidden_sectors;
    uint32_t zero4;
    uint32_t zero5;
    uint64_t total_sectors;
    uint64_t total_sectors_large;
    uint64_t mft_cluster; // mft cluster number
    uint64_t mft_mirror_cluster;
    int8_t clusters_per_mft_record;
    uint8_t zero6[3];
    int8_t clusters_per_index_record;
    uint8_t zero7[3];
    uint64_t volume_serial_number;
    uint32_t checksum;
    char bootstrap_code[426];
    uint16_t end_of_sector_marker;
};

struct DataInfo {
	int fileSize;
};

// Function to calculate byte address from LBA
uint64_t calculateByteAddress(uint32_t lba) {
    // sector size of 512 bytes
    return (uint64_t)lba * 512;
}

uint64_t clusterToByteAddress(uint32_t clusterNumber) {
	return (uint64_t)clusterNumber * 4096;
}

// Function to get the partition address from MBR
uint32_t getPartAdd(int fd, int partitionNumber) {
    struct MBRPartitionEntry entry;
    off_t mbrOffset = 0x1BE + (partitionNumber - 1) * sizeof(entry); // MBR entry offset

    if (lseek(fd, mbrOffset, SEEK_SET) == -1 || read(fd, &entry, sizeof(entry)) != sizeof(entry)) {
        perror("Error reading MBR");
        exit(EXIT_FAILURE);
    }

    return entry.lba_start;
}


uint64_t getMftAddr(int fd, uint64_t partitionByteAddress) {
	struct NTFSBootRecord br;
	
	off_t ntfsOffset = partitionByteAddress - 0x10;
	
	if (lseek(fd, ntfsOffset, SEEK_SET) == -1 || read(fd, &br, sizeof(br)) != sizeof(br)) {
	perror("Error reading NTFS BR");
	exit(EXIT_FAILURE);
	}
	
	
	return br.mft_cluster;
}

char* decimalToHex(int decimalNumber) {
    // Determine the maximum number of digits needed for the hexadecimal representation
    int maxDigits = snprintf(NULL, 0, "%X", decimalNumber);
    
    // Allocate memory for the hexadecimal string
    char* hexString = (char*)malloc(maxDigits + 1);
    
    // Convert the decimal number to hexadecimal
    snprintf(hexString, maxDigits + 1, "%X", decimalNumber);
    
    return hexString;
}

void getAttributes(int fd, uint64_t fileAddress) {
if(lseek(fd, fileAddress, SEEK_SET) == -1) {
    	perror("Can't find entry");
    	close(fd);
    	return;
    }
    
    char entryBuffer[1024];
    ssize_t fullEntry = read(fd, entryBuffer, 1024);
    if (fullEntry == -1) {
    	perror("Could not read entry");
    	close(fd);
    	return;
    }
    
    if (fullEntry != 1024) {
    	printf("Expected %d bytes, but only read %zd bytes\n", 1024, fullEntry);
    	close(fd);
    	return;
    }
    
    char attribute_offset = (unsigned char)entryBuffer[20];
    char* attributesStart = entryBuffer + attribute_offset;
    
    while (attributesStart < entryBuffer + 1024) {  
    	
  	uint32_t attributeType = *(uint32_t*)attributesStart;
        uint32_t attributeLength = *(uint32_t*)(attributesStart + 4);
        
        if(attributeLength == 0 || attributeType == 0xFFFFFFFF) {
    		break;
    	}
    	
    	char* hexAttributeLength = decimalToHex(attributeLength);
        printf("Attribute Type: 0x%08X, Length: %s\n", attributeType, hexAttributeLength);
  	
    	attributesStart += attributeLength;
    	
    }
}


// 00 00 - deleted file
// 01 00 - allocated file
// 02 00 - deleted directory
// 03 00 - allocated directory

void getAllocation(int fd, uint64_t fileAddress) {
if(lseek(fd, fileAddress, SEEK_SET) == -1) {
    	perror("Can't find entry");
    	close(fd);
    	return;
    }
    
    char entryBuffer[1024];
    ssize_t fullEntry = read(fd, entryBuffer, 1024);
    if (fullEntry == -1) {
    	perror("Could not read entry");
    	close(fd);
    	return;
    }
    
    if (fullEntry != 1024) {
    	printf("Expected %d bytes, but only read %zd bytes\n", 1024, fullEntry);
    	close(fd);
    	return;
    }
    
    printf("\nIn use or deleted & file or directory:\n");
    char attribute_offset = (unsigned char)entryBuffer[0x16];
    printf("Flag: 0%x\n", attribute_offset);
    
    switch(attribute_offset) {
    	case 0x00:
    	printf("File that is deleted");
    	break;
    	case 0x01:
    	printf("File that is in use\n");
    	break;
    	case 0x02:
    	printf("Directory that is deleted");
    	break;
    	case 0x03:
    	printf("Directory that is in use");
    	break;
    	default:
    	printf("Error");   	
    }
    
}

void getName(int fd, uint64_t fileAddress, char fileName[]) {
if(lseek(fd, fileAddress, SEEK_SET) == -1) {
    	perror("Can't find entry");
    	close(fd);
    	return;
    }
    
    char entryBuffer[1024];
    ssize_t fullEntry = read(fd, entryBuffer, 1024);
    if (fullEntry == -1) {
    	perror("Could not read entry");
    	close(fd);
    	return;
    }
    
    if (fullEntry != 1024) {
    	printf("Expected %d bytes, but only read %zd bytes\n", 1024, fullEntry);
    	close(fd);
    	return;
    }
    
    char attribute_offset = (unsigned char)entryBuffer[20];
    char* attributesStart = entryBuffer + attribute_offset;
    printf("%s\n", attributesStart);
    
    
    while (attributesStart < entryBuffer + 1024) {  
    	
  	uint32_t attributeType = *(uint32_t*)attributesStart;
        uint32_t attributeLength = *(uint32_t*)(attributesStart + 0x04);
        
        if(attributeType == 0x30) {
        	uint32_t parentEntryNumber = *(uint32_t*) (attributesStart + 0x18);
        	uint8_t fileNameLength = *(uint8_t*) (attributesStart + 0x58);
                printf("Name Attribute: 0x%02x, Length: %02x\n", attributeType, attributeLength);
                printf("Parent Entry: 0x%02x\n", parentEntryNumber);
                printf("File Name Length: 0x%02x\n", fileNameLength);
                
                printf("File Name: ");
                // Ox5A is the start, we need to increment by 2 and subtract 0x01 fron 0x1b until it hits 0x0  
                int increment = 0;

		int count = 0;
                while(fileNameLength > 0x0) {
                uint8_t fileNameChar = *(uint8_t*) (attributesStart + 0x5A + increment);
                printf("%c", fileNameChar);
                char convert= (char) fileNameChar;
                strncat(fileName, &convert, 1);
                fileNameLength -= 0x01;
               	increment += 2;
                }
                
    		break;
    	}
  	
    	attributesStart += attributeLength;
    	
    }
}

void getData(int fd, uint64_t fileAddress, int *fileSizeMain, int* startClusterNum, int* totalNumClusters) {
if(lseek(fd, fileAddress, SEEK_SET) == -1) {
    	perror("Can't find entry");
    	close(fd);
    	return;
    }
    
    char entryBuffer[1024];
    ssize_t fullEntry = read(fd, entryBuffer, 1024);
    struct DataInfo data;
    
    if (fullEntry == -1) {
    	perror("Could not read entry");
    	close(fd);
    	return;
    }
    
    if (fullEntry != 1024) {
    	printf("Expected %d bytes, but only read %zd bytes\n", 1024, fullEntry);
    	close(fd);
    	return;
    }
    
    char attribute_offset = (unsigned char)entryBuffer[20];
    char* attributesStart = entryBuffer + attribute_offset;
        	printf("%s\n", attributesStart);
    while (attributesStart < entryBuffer + 1024) {  
    	
  	uint32_t attributeType = *(uint32_t*)attributesStart;
        uint32_t attributeLength = *(uint32_t*)(attributesStart + 0x04);
        
        if(attributeType == 0x80) {
        	uint32_t parentEntryNumber = *(uint32_t*) (attributesStart + 0x18);
        	uint8_t residentFlag = *(uint8_t*) (attributesStart + 0x8);

                printf("Data Attribute: 0x%02x, Length: %02x\n", attributeType, attributeLength);   
                printf("Resident Flag: %02x\n", residentFlag);
                
                // if resident     
                if(residentFlag == 0x0) {
                	uint8_t fileSize = *(uint8_t*) (attributesStart + 0x10);
                	printf("File Size: %d B\n", fileSize);
                }
                
                if(residentFlag == 0x01) {
                	uint64_t fileSize = *(uint64_t*) (attributesStart + 0x30);
                	printf("File Size: %ld B\n\n", fileSize);
                	*fileSizeMain = fileSize;
                	
                	uint8_t offset = 0x40;
                	uint8_t dataRunPair = *(uint8_t*) (attributesStart + offset);
		
			while(dataRunPair != 0x00) {
			        printf("Data Run Pair: %x\n", dataRunPair);
				uint8_t tensDigit = (dataRunPair / 0x10) % 0x10;
				uint8_t onesDigit = dataRunPair % 0x10;
				printf("Size of Cluster Number: %x\n", tensDigit);
				printf("Size of Number of Clusters: %x\n", onesDigit);
				
				uint8_t increment = tensDigit + onesDigit;
				uint8_t lastByte = tensDigit;
				
				while(lastByte > 0x0) {
					uint8_t val = *(uint8_t*) (attributesStart + 0x40 + increment);		
					*startClusterNum = (*startClusterNum << 8) | val;

					lastByte -= 0x1;
					increment -= 0x1;
				}
				printf("Start Cluster Number: 0x%x\n", *startClusterNum);
				
				lastByte = onesDigit;
				increment = onesDigit;

				while(lastByte > 0x0) {
					uint8_t val = *(uint8_t*) (attributesStart + 0x40 + increment);		
					*totalNumClusters = (*totalNumClusters << 8) | val;
					
					lastByte -= 0x1;
					increment -= 0x1;
				}
				printf("Number of Clusters: 0x%x\n", *totalNumClusters);
				
				offset = offset + tensDigit + onesDigit + 0x1;
                		dataRunPair = *(uint8_t*) (attributesStart + offset);
                		printf("\n");
			}
                }
    		break;
    	}
  	
    	attributesStart += attributeLength;
    	
    }
}

void recoverFile(int fd, int fileSize, int startClusterNum, int totalNumClusters, char fileName[]) {
	printf("Beginning file recovery.\n");
	
	int newFile = open(fileName, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	
	int startClusterNumHex = startClusterNum * 0x1000;
	
	// start at this cluster, read, and write it to the file, continue until cluster count is reached.
	off_t seekAddress = startClusterNum * 0x1000;
	
	
	for(int i = 0; i < totalNumClusters; i++) {
		
		if(lseek(fd, seekAddress + (i * BUFFER_SIZE), SEEK_SET) == -1) {
			perror("Error using lseek");
			close(fd);
			exit(EXIT_FAILURE);
		}
	
		char buffer[BUFFER_SIZE];
		ssize_t bytesRead = read(fd, buffer, BUFFER_SIZE);

		if (bytesRead == -1) {
			perror("Error reading from NTFS partition");
			close(fd);
			exit(EXIT_FAILURE);
		}
	
		
		if (i == totalNumClusters - 1) {
			ssize_t bytesWritten = write(newFile, buffer, fileSize);
			if (bytesWritten == -1) {
				perror("Error writing to the new file");
				close(fd);
				close(newFile);
				exit(EXIT_FAILURE);
			}
		} else {
			ssize_t bytesWritten = write(newFile, buffer, bytesRead);
			if (bytesWritten == -1) {
				perror("Error writing to the new file");
				close(fd);
				close(newFile);
				exit(EXIT_FAILURE);
			}
		}
		
		fileSize = fileSize - BUFFER_SIZE;
	}
	
	close(newFile);
	printf("File Recovery Finished\n");
}


int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <device> <partition_number> <entry number>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *devicePath = argv[1];
    int partitionNumber = atoi(argv[2]);
    int entryNumber = atoi(argv[3]);

    int fd = open(devicePath, O_RDONLY);
    if (fd == -1) {
        perror("Error opening device");
        return EXIT_FAILURE;
    }

    uint32_t partitionAddress = getPartAdd(fd, partitionNumber);
    uint64_t byteAddress = calculateByteAddress(partitionAddress);

    printf("The Partition starts at address 0x%llx\n", (unsigned long long)byteAddress);
    
    uint64_t mftAddress = getMftAddr(fd, byteAddress);
    uint64_t mftByteAddress = clusterToByteAddress(mftAddress) + byteAddress; 
    
    printf("The MFT starts at address 0x%llx\n", (unsigned long long)mftByteAddress);


    uint64_t fileAddress = (entryNumber * 0x400) + mftByteAddress;
    
    printf("Entry %d starts at address 0x%llx\n", entryNumber, (unsigned long long)fileAddress);
    
    getAllocation(fd, fileAddress);
    
    char fileName[100] = "";
    getName(fd, fileAddress, fileName);
    printf("\n");
    
    int fileSize = 0;
    int startClusterNum = 0;
    int totalNumClusters = 0;
    getData(fd, fileAddress, &fileSize, &startClusterNum, &totalNumClusters);
    
    printf("\n");
    
    char response;
    printf("Would you like to recover the file? (Y/n):");
    scanf(" %c", &response);
    printf("\n");
    
    if((response == 'y') || (response == 'Y')) {
    	recoverFile(fd, fileSize, startClusterNum, totalNumClusters, fileName);
    }
    
    close(fd);
    return EXIT_SUCCESS;
}

