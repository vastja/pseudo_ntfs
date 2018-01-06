#ifndef _PSEUDO_NTFS_HPP_
#define _PSEUDO_NTFS_HPP_

#include <iostream>
#include <fstream>
#include <list>
#include <thread>
#include <semaphore.h>

    const int32_t UID_ITEM_FREE = 0;
    const int32_t MFT_FRAGMENTS_COUNT = 32;

    struct boot_record {
        char signature[9];              //login autora FS
        char volume_descriptor[251];    //popis vygenerovaného FS
        int32_t disk_size;              //celkova velikost VFS
        int32_t cluster_size;           //velikost clusteru
        int32_t cluster_count;          //pocet clusteru
        int64_t mft_start_address;      //adresa pocatku mft
        int64_t bitmap_start_address;   //adresa pocatku bitmapy
        int64_t data_start_address;     //adresa pocatku datovych bloku
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

            // CONSISTENCY CHECK PROPERTIES 
            const int SLAVES_COUNT = 4;
            const int MFT_ITEMS_PER_SLAVE = 1;
            bool isCorrupted;

            sem_t semaphore;

            int32_t lastCheckedMftItemIndex = 0;
            // -------------------------------- 

            /* internal variables */
            const int32_t mftItemsCount;
            int32_t uidCounter;

            /* infromations about free space and mft items*/
            int32_t freeSpace;
            int32_t freeMftItems;

            /* starts of important disk parts*/
            unsigned char * ntfs;
            struct boot_record * bootRecord;
            struct mft_item * mftItemStart;
            unsigned char * bitmapStart;
            unsigned char * dataStart;
            
            /* global flag for index out of range 
             * set in case you pass to function invalid disk index
            */
            bool indexOutOfRange;

            // initialize mft items to be free
            void initMft();
            /* initialize bitmap to be free
            */
            void initBitmap();
            /* set free/use for data cluster
             * can set index out of borders flag
             * +param index - data cluster index
             * +param value - free - false, used - true  
            */ 
            void setBitmap(const int index, const bool value);
            /* can set index out of borders flag
             * +return free - true, used - false
            */
            bool const isClusterFree(const int index);

            /* save mft item to mft items table
             * can set index out of borders flag
             * +param index - mft items table index
             * +param item - pointer to mft items we want to save to mft table
            */
            void setMftItem(const int index, const struct mft_item * item);
            /* free mft item from mft item table
             * can set index out of borders flag
             * param mftItemIndex - index in mft items table
            */
            void freeMftItem(const int32_t mftItemIndex);
            /* free mft item from mft item table and clear its data clusters
             * can set index out of borders flag
             * +param mftItemIndex - index in mft items table
            */
            void freeMftItemWithData(const int32_t mftItemIndex);

            /* set data in data cluster
             * can set index out of borders flag
             * +param index - data cluster index
             * +param data - data to be saved
             * +param size - count of bytes to be saved
            */
            void setClusterData(const int index, const unsigned char * data, const int size);
            /* get data from data cluster
             * can set index out of borders flag
             * +param index - data cluster we get data from
             * +param data - here will be data form data cluster saved
            */
            void getClusterData(const int index, unsigned char * data);
            /* clear data cluster
             * can set index out of borders flag
             * +param - startIndex - data cluster index
             * +param - clustersCount - how many data cluster from given index
            */
            void clearClusterData(const int startIndex, const int32_t clustersCount);

            /* return UID
             * +return - UID 
            */
            const int getUid() {return uidCounter++;};
            /* remove UID from directory
             * can set index out of borders flag
             * + param - directoryMftItemIndex - index of directory mft item in mft items table, we want to remove UID from
             * +param - uid - UID to be removed
            */
            void removeUidFromDirectory(const int32_t directoryMftItemIndex, int32_t uid);
            /* save UID to given mft item data clusters
             * can set index out of borders flag
             * +param - destinationMftItemIndex - mft item where should be UID saved
             * +param - uid - UID to be saved
            */
            bool saveUid(int32_t destinationMftItemIndex, int32_t uid);
            /* try to remove UID from data cluster, in case given UID is there
             * can set index out of borders flag
             * +param - startIndex - index of first data cluster
             * +param - clusterCount - count of culuster to be searched
             * +param - uid - UID to be deleted
             * +return true - given uid was found in given data clusters range and deleted, else false
            */
            bool removeUid(int32_t startIndex, int32_t clusterCount, int32_t uid); 
            /* try to write UID to data cluster, in case there is enough free space
             * can set index out of borders flag
             * +param - startIndex - index of first data cluster
             * +param - clusterCount - count of culuster to be searched
             * +param - uid - UID to be saved
             * +return true - given uid was saved, else false
            */
            bool writeUid(int32_t startIndex, int32_t clusterCount, int32_t uid);
            
            /* clear mft fragments
             * +param - fragments - array of mft fragments
            */
            void clearMftItemFragments(mft_fragment * fragments) const;
            /* find free mft item
            */
            int findFreeMft() const;
            /* count how many mft items we need to save given count of data clusters
             * +param - dataClustersCount - count of data clusters we need to save
             * +return count of needed mft fragment to save given count of data clusters, or NOT_FOUND
            */
            int neededMftItems(int8_t dataClustersCount) const;
            /* find maximal continula free space in bytes
             * can set index out of borders flag
             * +param - demandedSize - ideal length of continual free space in bytes
             * +param - startIndex - index of first data cluster in found free space
             * +param - providedSize - found maximal continual free space 
            */
            void findFreeSpace(const int32_t demandedSize, int32_t * startIndex, int32_t * providedSize);
            /* save continula data
             * can set index out of borders flag
             * +param - data - data to be saved
             * +param - size - size of data to be saved in bytes
             * +param - startIndex - index of first data cluster for data saving
            */
            void saveContinualSegment(const char * data, const int32_t size, const int32_t startIndex);
            /* prepare list with data segmets - start index and size in bytes - for demanded data size we want to save
             * TODO - check find free space not 0
             * +param - dataSegmentList - list of prepared data segments
             * +param - demandedSize - size of content to be saved in bytes 
            */
            void prepareMftItems(std::list<struct data_seg> * dataSegmentList, int32_t demandedSize);
            /* save file to ntfs
             * +param - dataSegmentList - list of prepared data segments
             * +param - fileName - name of file
             * +param - uid - UID of file
             * +param - fileData - content of file
             * +param - fileLength - size of file in bytes
            */
            void save(std::list<struct data_seg> * dataSegmentList, const char * fileName, int32_t uid, char * fileData, int32_t fileLength);
            
            int32_t searchClusters(const int32_t dataClusterStartIndex, const int32_t dataClustersCount, const char * name, const bool directory);
            int32_t searchCluster(const int32_t dataClusterIndex, const char * name, const bool directory); 
            int32_t findMftItemWithProperties(const int32_t uid, const char * name, const bool directory);

            void loadDataFragment(int32_t startIndex, int32_t fragmentCount, std::ostringstream * oss);

            void getAllUidsFromFragment(const int32_t startIndex, const int32_t fragmentsCount, std::list<int32_t> * uids);
            int32_t findMftItemWithUid(const int32_t uid);

            bool isDirEmpty(const int32_t mftItemIndex);

            /* ADVANCE FUNCTIONS */
            void consistencyCheckSlave();
            bool getMftItemsToCheck(int32_t * mftItemStartIndex, int32_t * mftItemEndIndex);
            int32_t getFileDataFragmentUsedSize(int32_t dataClusterStartIndex, int32_t dataClustersCount);
            int32_t getDirectoryDataFragmentUsedSize(int32_t dataClusterStartIndex, int32_t dataClustersCount);
            // -------------------
            void defragmentUpdateMftTable();
            void prepareIndextable();
            void prepareIndexTable(int32_t indextable[]);
            void fillIndexTable(int32_t indexTable[] ,const int32_t startIndex, const int32_t count, const int32_t startWithIndex);
            void defragment(int32_t indexTable[], int32_t checkIndex); 
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
            void move(const int32_t fileMftItemIndex, const int32_t fromMftItemIndex, const int32_t toMftItemIndex);
            void removeFile(const int32_t mftItemIndex, const int32_t parentDirectoryMftItemIndex);
            void copy(const int32_t fileMftItemIndex, int32_t toMftItemIndex);
            /* ADVANCE */
            bool checkDiskConsistency();
            void defragmentDisk();
            /* TEST FUNCTION */
            void printDisk();
            void printMftItemInfo(const mft_item * mftItem);
    };

#endif