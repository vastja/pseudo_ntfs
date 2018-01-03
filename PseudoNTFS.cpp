#include <cstring>
#include <math.h>
#include <iostream>
#include <string>
#include <sstream>

#include "PseudoNTFS.hpp"
#include "Utils.hpp"

PseudoNTFS::PseudoNTFS(int32_t diskSize, int32_t clusterSize) : mftItemsCount((diskSize * 0.1) / sizeof(mft_item)) {

    // initialize uid counter to 0
    uidCounter = 1;
    
    struct boot_record br;
    // set signature and description of volume
    strcpy(br.signature, "vastja");
    strcpy(br.volume_descriptor, "descripton");

    br.disk_size = diskSize;
    br.cluster_size = clusterSize;
    
    // 10% of disk space is for mft items
    // Set free mft items to mft items count
    freeMftItems = mftItemsCount;
    // rest of space is for clusters and bitmap
    int32_t clusterCount = floor((br.disk_size - sizeof(boot_record) - mftItemsCount * sizeof(mft_item)) / (0.125 + br.cluster_size)); 
    br.cluster_count = clusterCount;

    // free space in data segment
    freeSpace = clusterCount * br.cluster_size;

    //  disk is represented with byte array
    ntfs = new unsigned char[br.disk_size];
    memset(ntfs, 0, br.disk_size);
    bootRecord = (boot_record *) ntfs;

    //set start address for parts of disk
    br.mft_start_address = ((int32_t) ntfs) + sizeof(boot_record);
    mftItemStart = (mft_item *) br.mft_start_address;
    br.bitmap_start_address = br.mft_start_address + mftItemsCount * sizeof(mft_item);
    bitmapStart = (unsigned char *) br.bitmap_start_address;
    br.data_start_address = br.bitmap_start_address + ceil(br.cluster_count / 8.0);
    dataStart = (unsigned char *) br.data_start_address;

    br.mft_max_fragment_count = MFT_FRAGMENTS_COUNT;

    // set boot record for disk
    memcpy(ntfs, &br, sizeof(boot_record));

    initMft();
    initBitmap();

    // create root directory
    struct mft_item mftItem;
    mftItem.uid = getUid();
    mftItem.isDirectory = true;
    mftItem.item_order = 1;
    mftItem.item_order_total = 1;
    strcpy(mftItem.item_name, ROOT_NAME);
    mftItem.item_size = 0;
    // save root directory to start of mft table
    clearMftItemFragments(mftItem.fragments);
    setMftItem(0, &mftItem);

}

PseudoNTFS::~PseudoNTFS() {
    delete [] ntfs;
}

void PseudoNTFS::initMft() {


    struct mft_item tempMftItem = {UID_ITEM_FREE};

    struct boot_record * br = (boot_record *) ntfs;

    for (int i = 0; i < mftItemsCount; i++) {
        memcpy(&(((mft_item *) br->mft_start_address)[i]), &tempMftItem, sizeof(mft_item));
    }
}

// initialize bitmap to free (false = 0)
void PseudoNTFS::initBitmap() {

    for (int i = 0; i < bootRecord->cluster_count; i++) {
        setBitmap(i, false);
    }
}

void PseudoNTFS::setBitmap(const int index, const bool value) {


    if (index < 0 || index > bootRecord->cluster_count - 1) {
        indexOutOfRange = true;
        return;
    }

    int i = index / 8;
    int j = index % 8;

    unsigned char temp = bitmapStart[i];
    
    if (value) {
        temp = temp | (128 >> j);
    }
    else {
        temp = temp & (127 >> j);
    }

    memcpy(&bitmapStart[i], &temp, sizeof(unsigned char));
}

const bool PseudoNTFS::isClusterFree(const int index) {

    if (index < 0 || index > bootRecord->cluster_count - 1) {
        indexOutOfRange = true;
        return false;
    }

    int i = index / 8;
    int j = index % 8;
    unsigned char temp = bitmapStart[i];

    return  !((128 >> j) & temp);
}

void PseudoNTFS::setMftItem(const int index, const struct mft_item * item) {

    if (index < 0 || index > mftItemsCount - 1) {
        indexOutOfRange = true;
        return;
    }

    memcpy(&mftItemStart[index], item, sizeof(mft_item));

    freeMftItems--;
}

mft_item * PseudoNTFS::getMftItem(const int index) {

    if (index < 0 || index > mftItemsCount - 1) {
        indexOutOfRange = true;
        return NULL;
    }

    return &mftItemStart[index];
}

void PseudoNTFS::setClusterData(const int index, const unsigned char * data, const int size) {

    if (index < 0 || index > bootRecord->cluster_count - 1) {
        indexOutOfRange = true;
        return;
    }

    // clear cluster data
    memset(&dataStart[index * bootRecord->cluster_size], 0, bootRecord->cluster_size);

    // set cluster with data
    memcpy(&dataStart[index * bootRecord->cluster_size], data, size);
    setBitmap(index, true);
}

void PseudoNTFS::getClusterData(const int index, unsigned char * data) {
    
    if (index < 0 || index > bootRecord->cluster_count - 1) {
        indexOutOfRange = true;
        return;
    }

    memset(data, 0, bootRecord->cluster_size);
    memcpy(data, &dataStart[index * bootRecord->cluster_size], bootRecord->cluster_size);

}

void PseudoNTFS::prepareMftItems(std::list<struct data_seg> * dataSegmentList, int32_t demandedSize) {

        struct data_seg dataSegment;
        int32_t index, providedSize = 0;

        do {
            findFreeSpace(demandedSize, &index, &providedSize);

            if (demandedSize < providedSize) {
                dataSegment.size = demandedSize;
            }
            else {
                dataSegment.size = providedSize;
            }
            
            dataSegment.startIndex = index;

            dataSegmentList->push_back(dataSegment);
            demandedSize -= providedSize;
        } while (demandedSize > 0);
}

void PseudoNTFS::save(std::list<struct data_seg> * dataSegmentList, const char * fileName, int32_t uid, char * fileData, int32_t fileLength) {

        int8_t dataClustersCount = dataSegmentList->size();
       
        int8_t neededMftItemsCount = neededMftItems(dataClustersCount);
        if (neededMftItemsCount > freeMftItems) {
            std::cout << "NOT ENOUGH FREE ITEMS";
            return;
        }
        
        // Prepare struct to save
        struct mft_item mftItem;
        mftItem.uid = uid;
        mftItem.isDirectory = false;
        mftItem.item_order = 1;
        mftItem.item_order_total = mftItemsCount;
        strcpy(mftItem.item_name, fileName);
        mftItem.item_size = fileLength;

        int8_t counter, dataCounter = 0;
        int8_t mftItemsLeft = neededMftItemsCount;
        int8_t mftIndex;

        for (data_seg item : *dataSegmentList) {

            counter %= MFT_FRAGMENTS_COUNT;
            
            if (counter == 0) {
                mftIndex = findFreeMft();
                freeMftItems--;
                clearMftItemFragments(mftItem.fragments);  
            }

            mftItem.fragments[counter].fragment_start_address = item.startIndex;
            mftItem.fragments[counter].fragment_count = item.size / bootRecord->cluster_size;
            saveContinualSegment(fileData + dataCounter, item.size, item.startIndex);
            dataCounter += item.size;

            if (counter == MFT_FRAGMENTS_COUNT - 1) {
                mftItem.item_order++;
                setMftItem(mftIndex, &mftItem);
            }
    
            counter++;
        }
        setMftItem(mftIndex, &mftItem);

}

void PseudoNTFS::saveFileToPseudoNtfs(const char * fileName, const char * filePath, int32_t parentDirectoryMftIndex) {

        std::string fileData;
        if (!readFile(filePath, &fileData)) {
            std::cout << "FILE NOT FOUND";
            return;
        }
        // No end char - we only store values to save
        int len = fileData.length();

        char * data = new char[len];
        memcpy(data, fileData.c_str(), len);
        
        if (len > freeSpace) {
            std::cout << "NOT ENOUGH FREE SPACE";
            return;
        };
    
        int32_t uid = getUid();
        if (!saveUid(parentDirectoryMftIndex, uid)) {
            std::cout << "NOT ENOUGH FREE SPACE";
            return;
        }

        std::list<struct data_seg> dataSegmentList;
        prepareMftItems(&dataSegmentList, len);
        save(&dataSegmentList, fileName, uid, data, len);
}

void PseudoNTFS::copy(const int32_t fileMftItemIndex, int32_t toMftItemIndex) {

        struct mft_item * mftItem = &mftItemStart[fileMftItemIndex];

        std::string content;
        loadFileFromPseudoNtfs(fileMftItemIndex, &content);
        // No end char - we only store values to save
        int len = content.length();

        char * data = new char[len];
        memcpy(data, content.c_str(), len);
        
        if (len > freeSpace) {
            std::cout << "NOT ENOUGH FREE SPACE";
            return;
        };

        int32_t uid = getUid();
        if (!saveUid(toMftItemIndex, uid)) {
            std::cout << "NOT ENOUGH FREE SPACE";
            return;
        }

        std::list<struct data_seg> dataSegmentList;
        prepareMftItems(&dataSegmentList, len);
        save(&dataSegmentList, mftItem->item_name, uid, data, len);
}

void PseudoNTFS::clearMftItemFragments(mft_fragment * fragments) const {

    mft_fragment clearFragments[MFT_FRAGMENTS_COUNT] = {0, 0};
    memcpy(fragments, clearFragments, MFT_FRAGMENTS_COUNT * sizeof(mft_fragment));
}

int PseudoNTFS::neededMftItems(int8_t dataClustersCount) const {

    int8_t count = 0;

    do {  
        dataClustersCount -= MFT_FRAGMENTS_COUNT;
        count++;
    } while (dataClustersCount > 0);

    return count;
}

int PseudoNTFS::findFreeMft() const {

    struct mft_item * mftItem = mftItemStart;

    for (int i = 0; i < mftItemsCount; i++) {
        if (mftItem[i].uid == UID_ITEM_FREE) {
            return i;
        }
    }

    // In case there is no free mft item
    return NOT_FOUND;
}

/*
demandedSize - size we need to save whole file
startIndex - index to cluster data segment
providedSize - meximal continual free space
*/
void PseudoNTFS::findFreeSpace(const int32_t demandedSize, int32_t * startIndex, int32_t * providedSize) {

    *providedSize = 0;
    
    int32_t maxSize = 0;
    int32_t maxIndex = 0;

    int32_t bound = ceil(bootRecord->cluster_count / 8); 
    for (int i = 0; i < bound; i++) {
        if (isClusterFree(i)) {
            if (maxSize == 0) {
                maxIndex = i;
            }
            maxSize += bootRecord->cluster_size;
        }
        else {
            if (maxSize > *providedSize) {
                *providedSize = maxSize;
                *startIndex = maxIndex;
            }
            maxSize = 0; 
        }

        if (maxSize >= demandedSize) {
            *providedSize = maxSize;
            *startIndex = maxIndex;
            return;    
        } 
    }
}

/*
startIndex - index to cluster data segment
size - size of data for save
data - start of data to be saved
*/
void PseudoNTFS::saveContinualSegment(const char * data, const int32_t size, const int32_t startIndex) {
    
    int32_t bound = size / bootRecord->cluster_size;
    int32_t left = size;
    
    for (int i = 0; i < bound; i++) {  
        setClusterData(startIndex + i, (unsigned char *) data + i * bootRecord->cluster_size , bootRecord->cluster_size);
        left -= bootRecord->cluster_size;
    }

    if (left > 0) {
        setClusterData(startIndex + bound, (unsigned char *) data + bound * bootRecord->cluster_size, left);
    }
}


int32_t PseudoNTFS::contains(const int32_t mftItemIndex, const char * name, const bool directory) {

    struct mft_item * mftItem = &mftItemStart[mftItemIndex];
    
    int32_t dataClusterStartIndex, dataClustersCount, searchedMftItemIndex;
    for (int i = 0; i < MFT_FRAGMENTS_COUNT; i++) {
        dataClusterStartIndex = mftItem->fragments[i].fragment_start_address;
        dataClustersCount = mftItem->fragments[i].fragment_count;

        searchedMftItemIndex = searchClusters(dataClusterStartIndex, dataClustersCount, name, directory);
        if (searchedMftItemIndex != NOT_FOUND) {
            return searchedMftItemIndex;
        }
    }

    return NOT_FOUND;

}

int32_t PseudoNTFS::searchClusters(const int32_t dataClusterStartIndex, const int32_t dataClustersCount, const char * name, const bool directory) {
    
    int32_t mftItemIndex;
    for (int32_t i = dataClusterStartIndex; i < dataClusterStartIndex + dataClustersCount; i++) {
        mftItemIndex = searchCluster(i, name, directory);
        if (mftItemIndex != NOT_FOUND) {
            return mftItemIndex;
        }
    }

    return NOT_FOUND;
    
}

int32_t PseudoNTFS::searchCluster(const int32_t dataClusterIndex, const char * name, const bool directory) {

    int maxUidInDataCluster = bootRecord->cluster_size / sizeof(int32_t);
    int32_t * mftItemUid = (int32_t *) &dataStart[dataClusterIndex * bootRecord->cluster_size];

    int32_t mftItemIndex;
    for (int i = 0; i < maxUidInDataCluster; i++) {
        mftItemIndex = findMftItemWithProperties(mftItemUid[i], name, directory);

        if (mftItemIndex != NOT_FOUND) {
            return mftItemIndex;
        }
    }

    return NOT_FOUND;  
}

int32_t PseudoNTFS::findMftItemWithProperties(const int32_t uid, const char * name, const bool directory) {

    struct mft_item * tempMftItem = mftItemStart;
    for (int i = 0; i < mftItemsCount; i++) {
        if (tempMftItem[i].uid == uid && strcmp(tempMftItem[i].item_name, name) == 0 && tempMftItem[i].isDirectory == directory) {
            return i;
        }
    }

    return NOT_FOUND;
}

bool PseudoNTFS::saveUid(int32_t destinationMftItemIndex, int32_t uid) {

    struct mft_item * mftItem = &mftItemStart[destinationMftItemIndex];

    int32_t fragmentCount;
    int32_t size = mftItem->item_size;
    for (int i = 0; i < MFT_FRAGMENTS_COUNT; i++) {

        fragmentCount = mftItem->fragments[i].fragment_count;
        if (fragmentCount > 0) {
            if (writeUid(mftItem->fragments[i].fragment_start_address, fragmentCount, uid)) {
                mftItem->item_size += sizeof(int32_t);
                return true;
            }
        }
        else if (fragmentCount == 0) {
            int32_t providedSize = 0;
            int32_t startIndex = 0;

            findFreeSpace(bootRecord->cluster_size, &startIndex, &providedSize);

            if (providedSize == 0 || providedSize < sizeof(int32_t)) {
                return false;
            }
            else {
                mftItem->item_size += sizeof(int32_t);
                mftItem->fragments[i].fragment_count = 1;
                mftItem->fragments[i].fragment_start_address = startIndex;
                writeUid(startIndex, 1, uid);
                setBitmap(startIndex, true);
                return true;
            }
        }
    }

    return false;
}

bool PseudoNTFS::writeUid(int32_t startIndex, int32_t clusterCount, int32_t uid) {
    
    int32_t * tempUid = (int32_t *)&dataStart[startIndex * bootRecord->cluster_size]; 

    for (int i = 0; i < clusterCount; i++) {
        if (tempUid[i] == 0) {
            tempUid[i] = uid;
            return true;
        }
    }

    return false;
}

void PseudoNTFS::loadFileFromPseudoNtfs(int32_t mftItemIndex, std::string * content) {

    struct mft_item * mftItem = &mftItemStart[mftItemIndex];

    std::ostringstream oss;
    for (int i = 0; i < MFT_FRAGMENTS_COUNT; i++) {
        loadDataFragment(mftItem->fragments[i].fragment_start_address, mftItem->fragments[i].fragment_count, &oss);
    }

    *content = oss.str();
}

void PseudoNTFS::loadDataFragment(int32_t startIndex, int32_t fragmentCount, std::ostringstream * oss) {

    char * buffer = new char[bootRecord->cluster_size + 1];
    buffer[bootRecord->cluster_size] = '\0';

    for (int i = startIndex; i < fragmentCount; i++) {
        getClusterData(i, (unsigned char *) buffer);
        *oss << buffer;
    }
    delete [] buffer;
    buffer = NULL;
}

void PseudoNTFS::getDirectoryContent(const int32_t directoryMftItemIndex, std::list<mft_item> * content) {

    struct mft_item * directoryMftItem = &mftItemStart[directoryMftItemIndex];

    std::list<int32_t> uids;

    for (int i = 0; i < MFT_FRAGMENTS_COUNT; i++) {
        getAllUidsFromFragment(directoryMftItem->fragments[i].fragment_start_address, directoryMftItem->fragments[i].fragment_count, &uids);
    }

    struct mft_item * mftItem = mftItemStart;
    int32_t mftItemIndex;

    for (int32_t uid : uids) {
        mftItemIndex = findMftItemWithUid(uid);
        if (mftItemIndex != NOT_FOUND) {
            content->push_back(mftItem[mftItemIndex]);
        }
    }

}

void PseudoNTFS::getAllUidsFromFragment(const int32_t startIndex, const int32_t fragmentsCount, std::list<int32_t> * uids) {

    int32_t * tempUid = (int32_t *) &dataStart[startIndex * bootRecord->cluster_size];
    int32_t bound = (fragmentsCount * bootRecord->cluster_size) / sizeof(int32_t);

    for (int i = 0; i < bound; i++) {
        if (tempUid[i] != 0) {
            uids->push_back(tempUid[i]);
        }    
    }
}

int32_t PseudoNTFS::findMftItemWithUid(const int32_t uid) {

    for (int i = 0; i < mftItemsCount; i++) {
        if (mftItemStart[i].uid == uid) {
            return i;
        }
    }

    return NOT_FOUND;
}

void PseudoNTFS::makeDirectory(const int32_t parentMftItemIndex, const char * name) {

    if (freeSpace < bootRecord->cluster_size) {
        std::cout << "NOT ENOUGH FREE SPACE\n";
        return;
    }

    int32_t mftIndex = findFreeMft();

    if (mftIndex == NOT_FOUND) {
        std::cout << "NOT ENOUGH FREE MFT ITEMS\n";
        return;
    }
    else {
        freeMftItems--;
    }

    int32_t demandedSize, providedSize, startIndex;

    demandedSize = bootRecord->cluster_size;
    findFreeSpace(demandedSize, &startIndex, &providedSize);

    if (providedSize == 0) {
        std::cout << "NOT ENOUGH FREE SPACE\n";
        return;
    }

    struct mft_item mftItem;
    mftItem.uid = getUid();
    strcpy(mftItem.item_name, name);
    mftItem.isDirectory = true;
    mftItem.item_order = 1;
    mftItem.item_order_total = 1;
    mftItem.item_size = 0;
    clearMftItemFragments(mftItem.fragments);
    setMftItem(mftIndex, &mftItem);
    saveUid(parentMftItemIndex, mftItem.uid);

}

bool PseudoNTFS::isDirEmpty(const int32_t mftItemIndex) {
    return mftItemStart[mftItemIndex].item_size == 0;
}

void PseudoNTFS::removeDirectory(const int32_t mftItemIndex, const int32_t parentDirectoryMftItemIndex) {

    struct mft_item * mftItem = &mftItemStart[mftItemIndex];

    if (!isDirEmpty(mftItemIndex)) {
        std::cout << "NOT EMPTY\n";
        return;
    }
    else {
        removeUidFromDirectory(parentDirectoryMftItemIndex, mftItem->uid);
        freeMftItem(mftItemIndex);  
    }
}

void PseudoNTFS::freeMftItem(const int32_t mftItemIndex) {

    struct mft_item * mftItem = &mftItemStart[mftItemIndex];

    mftItem->uid = UID_ITEM_FREE;
    strcpy(mftItem->item_name, "");
    mftItem->item_size = 0;
    mftItem->item_order = 0;
    mftItem->item_order_total = 0;
    mftItem->isDirectory = false;
    clearMftItemFragments(mftItem->fragments);

    freeMftItems++;
}

void PseudoNTFS::freeMftItemWithData(const int32_t mftItemIndex) {

    struct mft_item * mftItem = &mftItemStart[mftItemIndex];

    for (int i = 0; i < MFT_FRAGMENTS_COUNT; i++) {
        if (mftItem->fragments[i].fragment_count != 0) {
            clearClusterData(mftItem->fragments[i].fragment_start_address, mftItem->fragments[i].fragment_count);
        }
    }

    freeMftItem(mftItemIndex);
}

void PseudoNTFS::removeUidFromDirectory(const int32_t directoryMftItemIndex, int32_t uid) {

    struct mft_item * mftItem = &mftItemStart[directoryMftItemIndex];

    for (int i =0; i < MFT_FRAGMENTS_COUNT; i++) {
        if (removeUid(mftItem->fragments[i].fragment_start_address, mftItem->fragments[i].fragment_count, uid)) {
            mftItem->item_size -= sizeof(int32_t);
            break;
        }
    }
}

bool PseudoNTFS::removeUid(int32_t startIndex, int32_t clusterCount, int32_t uid) {
    
    int32_t * tempUid = (int32_t *)&dataStart[startIndex * bootRecord->cluster_size]; 

    for (int i = 0; i < clusterCount; i++) {
        if (tempUid[i] == uid) {
            tempUid[i] = 0;
            return true;
        }
    }

    return false;
}

void PseudoNTFS::move(const int32_t fileMftItemIndex, const int32_t fromMftItemIndex, const int32_t toMftItemIndex) {

    struct mft_item * mftItem = &mftItemStart[fileMftItemIndex];

    if (saveUid(toMftItemIndex, mftItem->uid)) {
        removeUidFromDirectory(fromMftItemIndex, mftItem->uid);
    }
}

void PseudoNTFS::removeFile(const int32_t mftItemIndex, const int32_t parentDirectoryMftItemIndex) {

    struct mft_item * mftItem = &mftItemStart[mftItemIndex];

    freeMftItemWithData(mftItemIndex);
    removeUidFromDirectory(parentDirectoryMftItemIndex, mftItem->uid);

}

void PseudoNTFS::clearClusterData(const int startIndex, const int32_t clustersCount) {

    if (startIndex < 0 || startIndex + clustersCount > bootRecord->cluster_count - 1) {
        indexOutOfRange = true;
        return;
    }

    // clear cluster data
    memset(&dataStart[startIndex * bootRecord->cluster_size], 0, clustersCount * bootRecord->cluster_size);

    for (int i = startIndex; i < clustersCount; i++) {
        setBitmap(i, true);
    }
}

/* TEST FUNCTIONS */

void PseudoNTFS::printMftItemInfo(const mft_item * mftItem) {
    std::cout << "UID: " << mftItem->uid << std::endl;
    std::cout << "Name: " << mftItem->item_name << std::endl;
    std::cout << "Is directory: " << mftItem->isDirectory << std::endl;
    std::cout << "Item size: " << (int) mftItem->item_size << std::endl;
    std::cout << "Item order: " << (int) mftItem->item_order << std::endl;
    std::cout << "Item order total: " << (int) mftItem->item_order_total << std::endl;
}

void PseudoNTFS::printBootRecord(std::ofstream * output) const {

    struct boot_record * br =  (boot_record *) ntfs;

    *output << "Signature: "<< br->signature << std::endl;
    *output << "Volume descriptor: "<< br->volume_descriptor << std::endl;
    *output << "Disk size: "<< br->disk_size << std::endl;
    *output << "Cluster size: "<< br->cluster_size << std::endl;
    *output << "Cluster count: "<< br->cluster_count << std::endl;
    *output << "Mft start address: "<< br->mft_start_address << std::endl;
    *output << "Bitmap start address: "<< br->bitmap_start_address << std::endl;
    *output << "Data start address: "<< br->data_start_address << std::endl;
    *output << "Mft max fragment count: "<< br->mft_max_fragment_count << std::endl;

}

void PseudoNTFS::printMftItem(mft_item * mftItem, std::ofstream * output) const {

    *output << "UID: " << mftItem->uid << std::endl;
    *output << "Name: " << mftItem->item_name << std::endl;
    *output << "Is directory: " << mftItem->isDirectory << std::endl;
    *output << "Item size: " << mftItem->item_size << std::endl;
    *output << "Item order: " << mftItem->item_order << std::endl;
    *output << "Item order total: " << mftItem->item_order_total << std::endl;

} 

void PseudoNTFS::printDisk() {

    std::ofstream output;
    output.open("disk.txt");

    output << "--- DISK ---\n";
    struct boot_record * br = (boot_record *) ntfs;

    output << "--- BOOT REDCORD ---\n";
    printBootRecord(&output);
    
    output << "--- MFT TABLE ---\n";
    mft_item * mftItem = (mft_item *) br->mft_start_address;
    for (int i = 0; i < mftItemsCount; i++) {
        printMftItem(&mftItem[i], &output);
    }

    output << "--- BITMAP ---\n";
    for (int i = 0; i < br->cluster_count; i++) {
        output << isClusterFree(i) << std::endl;
    }

    output << "--- DATA CLUSTERS ---\n";
    char * data = new char[br->cluster_size + 1];
    data[br->cluster_size] = '\0';
    for (int i = 0; i < br->cluster_count; i++) {
        output << "--- " << i << ". CLUSTER ---\n";
        getClusterData(i, (unsigned char *) data);
        output << data << std::endl;
    }

    output.close();
}