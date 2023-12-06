# File Recovery Tool for NTFS

## Description
Tool used to recover deleted files from an NTFS file system.


## Getting Started

### Compilation
To use the tool, you must have gcc installed and compile it.
```
gcc recoverFile.c -o recoverFile
```

## Usage
To run the file, use the command:
```
./recoverFile <device> <partition number> <entry number>
```
to get the device:
```
lsblk
```
or
```
fdisk -l
```
to get the entry number of your file, navigate to the directory of the file and use the command:
```
ls -i
```

### Example
```
./recoverFile /dev/sdb 1 72
```

## Features
The tool will display the following information:
- Hex address of the MBR
- Hex address of the MFT
- Hex address of the entry
- File is deleted/in use
- Name of the file
- data information

It will then give the option to recover the file. If file recovery occurs, the recovered file will be saved in the current working directory.

## Roadmap/To Do
- refactor code
- take into account the possibility of negative data run offset
- additional input validation
- addiional test cases

## Disclaimer
- Since this tool works directly with the partition, the commands must be run using **sudo** within commands.
- This tool is for Linux systems only
- before attempting to recover a file it is important that you **unmount** the partition beforehand, could lead to errors otherwise.
  
## Conclusion
The creation of this tool allowed me to develop an in-depth understanding of the NTFS file system as well as how exactly files can be recovered in the case that they are deleted completely from the partition. It also developed my understanding of byte addresses and how files are generally stored in a file system.
