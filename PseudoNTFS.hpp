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

            /* CONSISTENCY CHECK PROPERTIES */
            const int SLAVES_COUNT = 4;
            const int MFT_ITEMS_PER_SLAVE = 1;
            bool isCorrupted;

            sem_t semaphore;

            int32_t lastCheckedMftItemIndex = 0;
            /********************************/

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
            
            /* search for clusters for directory/file with given name
             * can set index out of borders flag
             * +param - dataClusterStartIndex - index of first data cluster for search
             * +param - dataClustersCount - count of data clusters for search
             * +param - name - name of searched file/directory  
             * +param - directory - true - directory, false - file
             * +return mft item index or NOT_FOUND
            */
            int32_t searchClusters(const int32_t dataClusterStartIndex, const int32_t dataClustersCount, const char * name, const bool directory);
            /* search for cluster for directory/file with given name 
             * can set index out of borders flag
             * +param - dataClusterIndex - index of searched data cluster
             * +param - name - name of searched file/directory
             * +param - directory - true - directory, false - file
             * +return mft item index or NOT_FOUND
            */
            int32_t searchCluster(const int32_t dataClusterIndex, const char * name, const bool directory);
            /* search for mft item with given name, durectory/file and UID
             * +param - uid - file/directory UID
             * +param - name - file/directory name
             * +param - directory - directory - true , file - false
             * +return mft item index or NOT_FOUND
            */ 
            int32_t findMftItemWithProperties(const int32_t uid, const char * name, const bool directory);

            /* load data fragment
             * can set index out of borders flag
             * +param - startIndex - first index of data cluster for loading
             * +param - fragmentCount - count of data clusters for loading
             * +param - oss - stream for loading of content
            */
            void loadDataFragment(int32_t startIndex, int32_t fragmentCount, std::ostringstream * oss);
            /* get all UIDs from fragment
             * can set index out of borders flag
             * +param - startIndex - index of first data cluster
             * +param - fragmentsCount - count of data clusters from which are UIDs loaded
             * +param - uids - list where are stored uids from data clusters
            */
            void getAllUidsFromFragment(const int32_t startIndex, const int32_t fragmentsCount, std::list<int32_t> * uids);
            /* find mft item with UID
             * +param - uid - searched UID
             * +return mft item index with UID or NOT_FOUND 
            */
            int32_t findMftItemWithUid(const int32_t uid);

            /* check if directory is empty
             * can set index out of borders flag
             * +param - mftitemIndex - index of mft item to be checked
             * +return true - empty, else false
            */
            bool isDirEmpty(const int32_t mftItemIndex);

            /*********************/
            /* ADVANCE FUNCTIONS */
            /*********************/
            /* CONSISTENCY CHECK */
            /* sleave for consistency check
             * PARALEL RUN
            */
            void consistencyCheckSlave();
            /* distribute work to slaves - salve check items from start index to end index (include)
             * THREAD SAFE
             * +param - mftItemStartIndex - index of first mft item slave should check
             * +param - mftItemEndIndex - index of last mft item slave should check
             * * +return true - is somthing to check, else false
            */
            bool getMftItemsToCheck(int32_t * mftItemStartIndex, int32_t * mftItemEndIndex);
            /* get size of file in data clusters
             * can set index out of borders flag
             * +param - dataClusterStartIndex - index of first counted data cluster 
             * +param - dataClustersCount - count of counted data clusters
             * +return size of used space in data clusters
            */
            int32_t getFileDataFragmentUsedSize(int32_t dataClusterStartIndex, int32_t dataClustersCount);
            /* get size of directory in data clusters
             * can set index out of borders flag
             * +param - dataClusterStartIndex - index of first counted data cluster 
             * +param - dataClustersCount - count of counted data clusters
             * +return size of used space in data clusters
            */
            int32_t getDirectoryDataFragmentUsedSize(int32_t dataClusterStartIndex, int32_t dataClustersCount);
            /** DEFRAGMENTATION **/
            /* update mft table after defragmentation
            */
            void defragmentUpdateMftTable();
            /* count index table for defragmentation
             * +param - indexTable - index table
            */
            void prepareIndexTable(int32_t indextable[]);
            /* fill index table with values 
             * can set index out of borders flag
             * +param - indexTable - index table
             * +param - startIndex - start index in index table
             * +param - count - count of indexs filled into index table
             * +param - startWithIndex - start number (filled in index table)             
            */
            void fillIndexTable(int32_t indexTable[] ,const int32_t startIndex, const int32_t count, const int32_t startWithIndex);
            /* defragment data cluster
             * can set index out of borders flag
             * +param - indextable - index table
             * +param - checkIndex - data cluster we check (move)
            */
            void defragment(int32_t indexTable[], int32_t checkIndex); 
            /*********************/

            /*** TEST FUNCTION ***/
            void printBootRecord(std::ofstream * output) const;
            void printMftItem(mft_item * mftItem, std::ofstream * output) const;
            /*********************/

        public:

            PseudoNTFS(int32_t diskSize, int32_t clusterSize);
            ~PseudoNTFS();

            /* save file to ntfs
             * can set index out of borders flag
             * +param - fileName - name of file
             * +param - filePath - path to file
             * +param - parentDirectoryMftIndex - index of mfti item of directory where the file will be saved 
            */
            void saveFileToPseudoNtfs(const char * fileName, const char * filePath, int32_t parentDirectoryMftIndex);
            /* load file form ntfs
             * can set index out of borders flag
             * +param - mftItemIndex - index of mft itme of demanded file
             * +param - string for file loading 
            */
            void loadFileFromPseudoNtfs(int32_t mftItemIndex, std::string * content);

            /* clear error state
            */
            void clearErrorState() {indexOutOfRange = true;};
            /* get error state
             * +return true - index out of borders, else false
            */
            const bool getErrorState() {return indexOutOfRange;};

            /* contains mft item file/directory with given name
             * can set index out of borders flag
             * +param - mftItemIndex - index of mft item, function searched in
             * +param - name - name of searched file/directory
             * +param - directory - ture - directory, false - file
             * +return - mft item index with given properties, or NOT_FOUND
            */
            int32_t contains(const int32_t mftItemIndex, const char * name, const bool directory);
            /* get mft item with index
             * can set index out of borders flag
             * +param - index - index of demanded mft item
             * +return demanded mft item, or NULL
            */
            mft_item * getMftItem(const int index);
            /* get content of directory
             * can set index out of borders flag
             * +param - directoryMftitemIndex - mft index of directory content is from
             * +param - content - list for content saving
            */
            void getDirectoryContent(const int32_t directoryMftItemIndex, std::list<mft_item> * content);

            /* make directory
             * can set index out of borders flag
             * +param - perntMftitemIndex - index of mft item where directory will be created
             * +param - name - directory name
            */
            void makeDirectory(const int32_t parentMftItemIndex, const char * name);
            /* remove directory
             * can set index out of borders flag
             * +param - mftItemIndex - index of mft item for removal
             * +param - parentDirectoryMftItemindex - index of mft item where directory for remove is
            */
            void removeDirectory(const int32_t mftItemIndex, const int32_t parentDirectoryMftItemIndex);
            /* moev file to another directory
             * can set index out of borders flag
             * +param - fileMftItemIndex - index of mft item of file
             * +param - fromMftItemIndex - index origina directory mft item file is from
             * +param toMdtItemIndex - index of directory mft item file will be moved to
            */
            void move(const int32_t fileMftItemIndex, const int32_t fromMftItemIndex, const int32_t toMftItemIndex);
            /* remove file
             * can set index out of borders flag
             * +param - mftItemIndex - index of file mft item
             * +param - parentDirectoryMftitemIndex - index of directory mft item file is from
            */
            void removeFile(const int32_t mftItemIndex, const int32_t parentDirectoryMftItemIndex);
            /* copy file to directory
             * can set index out of borders flag
             * +param - fileMftitemIndex - index of file mft item for copying
             * +param - toMftItemIndex - index of destination mft item
            */
            void copy(const int32_t fileMftItemIndex, int32_t toMftItemIndex);
            /*********************/
            /* ADVANCE FUNCTIONS */
            /*********************/
            bool checkDiskConsistency();
            void defragmentDisk();
            /*********************/
            /*** TEST FUNCTION ***/
            void printDisk();
            void printMftItemInfo(const mft_item * mftItem);
            /*********************/
    };

#endif