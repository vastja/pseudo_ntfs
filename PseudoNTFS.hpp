#ifndef _PSEUDO_NTFS_HPP_
#define _PSEUDO_NTFS_HPP_

#include <iostream>
#include <fstream>
#include <list>

    const int32_t UID_ITEM_FREE = 0;
    const int32_t MFT_FRAGMENTS_COUNT = 32;

    struct boot_record {
        char signature[9];              //login autora FS
        char volume_descriptor[251];    //popis vygenerovaného FS
        int32_t disk_size;              //celkova velikost VFS
        int32_t cluster_size;           //velikost clusteru
        int32_t cluster_count;          //pocet clusteru
        int32_t mft_start_address;      //adresa pocatku mft
        int32_t bitmap_start_address;   //adresa pocatku bitmapy
        int32_t data_start_address;     //adresa pocatku datovych bloku
        int32_t mft_max_fragment_count; //maximalni pocet fragmentu v jednom zaznamu v mft (pozor, ne souboru)
                                        // stejne jako   MFT_FRAGMENTS_COUNT
    };

    struct mft_fragment {
        int32_t fragment_start_address;     //start adresa
        int32_t fragment_count;             //pocet clusteru ve fragmentu
    };

    struct mft_item {
        int32_t uid;                                        //UID polozky, pokud UID = UID_ITEM_FREE, je polozka volna
        bool isDirectory;                                   //soubor, nebo adresar
        int8_t item_order;                                  //poradi v MFT pri vice souborech, jinak 1
        int8_t item_order_total;                            //celkovy pocet polozek v MFT
        char item_name[12];                                 //8+3 + /0 C/C++ ukoncovaci string znak
        int32_t item_size;                                  //velikost souboru v bytech
        struct mft_fragment fragments[MFT_FRAGMENTS_COUNT]; //fragmenty souboru
    };

    const char ROOT_NAME[] = "root";

    struct data_seg {
        int32_t startIndex;
        int32_t size;
    };

    class PseudoNTFS {

        private:
            unsigned char * ntfs;
            const int32_t mftItemsCount;
            int32_t uidCounter;

            int32_t freeSpace;
            int32_t freeMftItems;

            struct boot_record * bootRecord;
            struct mft_item * mftItemStart;
            unsigned char * bitmapStart;
            unsigned char * dataStart;

            bool indexOutOfRange;

            void initMft();
            void initBitmap();
            void setBitmap(const int index, const bool value);
            bool const isClusterFree(const int index);

            void setMftItem(const int index, const struct mft_item * item);

            void setClusterData(const int index, const unsigned char * data, const int size);
            void getClusterData(const int index, unsigned char * data);

            const int getUid() {return uidCounter++;};
            
            void clearMftItemFragments(mft_fragment * fragments) const;
            int findFreeMft() const;
            int neededMftItems(int8_t dataClustersCount) const;
            void findFreeSpace(const int32_t demandedSize, int32_t * startIndex, int32_t * providedSize);
            void saveContinualSegment(const char * data, const int32_t size, const int32_t startIndex);
            void prepareMftItems(std::list<struct data_seg> * dataSegmentList, int32_t demandedSize);
            void save(std::list<struct data_seg> * dataSegmentList, const char * fileName, int32_t uid, char * fileData, int32_t fileLength);
            
            int32_t searchClusters(const int32_t dataClusterStartIndex, const int32_t dataClustersCount, const char * name, const bool directory);
            int32_t searchCluster(const int32_t dataClusterIndex, const char * name, const bool directory); 
            int32_t findMftItemWithProperties(const int32_t uid, const char * name, const bool directory);

            void removeUid(const int32_t destinationMftItemIndex, int32_t uid);
            bool saveUid(int32_t destinationMftItemIndex, int32_t uid);
            bool removeUid(int32_t startIndex, int32_t clusterCount, int32_t uid); 
            bool writeUid(int32_t startIndex, int32_t clusterCount, int32_t uid);

            void loadDataFragment(int32_t startIndex, int32_t fragmentCount, std::ostringstream * oss);

            void getAllUidsFromFragment(const int32_t startIndex, const int32_t fragmentsCount, std::list<int32_t> * uids);
            int32_t findMftItemWithUid(const int32_t uid);

            bool isDirEmpty(const int32_t mftItemIndex);

            /* TEST FUNCTION */
            void printBootRecord(std::ofstream * output) const;
            void printMftItem(mft_item * mftItem, std::ofstream * output) const;

        public:

            PseudoNTFS(int32_t diskSize, int32_t clusterSize);
            ~PseudoNTFS();

            void saveFileToPseudoNtfs(const char * fileName, const char * filePath, int32_t parentDirectoryMftIndex);
            void loadFileFromPseudoNtfs(int32_t mftItemIndex, std::string * content);

            void clearErrorState() {indexOutOfRange = true;};
            const bool getErrorState() {return indexOutOfRange;};

            int32_t contains(const int32_t mftItemIndex, const char * name, const bool directory);
            mft_item * getMftItem(const int index);

            void getDirectoryContent(const int32_t directoryMftItemIndex, std::list<mft_item> * content);

            void makeDirectory(const int32_t parentMftItemIndex, const char * name);
            void removeDirectory(const int32_t mftItemIndex, const int32_t parentDirectoryMftItemIndex);

            /* TEST FUNCTION */
            void printDisk();
            void printMftItemInfo(const mft_item * mftItem);
    };

#endif