#include <cstring>
#include <math.h>
#include <iostream>
#include <string>

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

    //set start address for parts of disk
    br.mft_start_address = ((int32_t) ntfs) + sizeof(boot_record);
    br.bitmap_start_address = br.mft_start_address + mftItemsCount * sizeof(mft_item);
    br.data_start_address = br.bitmap_start_address + ceil(br.cluster_count / 8.0);

    br.mft_max_fragment_count = MFT_FRAGMENTS_COUNT;

    // set boot record for disk
    memcpy(ntfs, &br, sizeof(boot_record));

    initMft();
    initBitmap();

    // create root directory
    struct mft_item mftItem;
    mftItem.uid = getUID();
    mftItem.isDirectory = true;
    mftItem.item_order = 1;
    mftItem.item_order_total = 1;
    strcpy(mftItem.item_name, ROOT_NAME);
    mftItem.item_size = 0;
    // save root directory to start of mft table
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

    struct boot_record * br = (boot_record *) ntfs;
    for (int i = 0; i < br->cluster_count; i++) {
        setBitmap(i, false);
    }
}

void PseudoNTFS::setBitmap(const int index, const bool value) {

    struct boot_record * br =  (boot_record *) ntfs;

    if (index < 0 || index > br->cluster_count - 1) {
        indexOutOfRange = true;
        return;
    }

    int i = index / 8;
    int j = index % 8;

    unsigned char temp = ((unsigned char *) br->bitmap_start_address)[i];
    
    if (value) {
        temp = temp | (128 >> j);
    }
    else {
        temp = temp & (127 >> j);
    }

    memcpy(&(((unsigned char *) br->bitmap_start_address)[i]), &temp, sizeof(unsigned char));
}

const bool PseudoNTFS::isClusterFree(const int index) {

    struct boot_record * br =  (boot_record *) ntfs;

    if (index < 0 || index > br->cluster_count - 1) {
        indexOutOfRange = true;
        return false;
    }

    int i = index / 8;
    int j = index % 8;
    unsigned char temp = ((unsigned char *) br->bitmap_start_address)[i];

    return  !((128 >> j) & temp);
}

void PseudoNTFS::setMftItem(const int index, const struct mft_item * item) {

    if (index < 0 || index > mftItemsCount - 1) {
        indexOutOfRange = true;
        return;
    }

    struct boot_record * br =  (boot_record *) ntfs;
    memcpy(&(((mft_item *) br->mft_start_address)[index]), item, sizeof(mft_item));

    freeMftItems--;
}

mft_item * PseudoNTFS::getMftItem(const int index) {

    if (index < 0 || index > mftItemsCount - 1) {
        indexOutOfRange = true;
        return NULL;
    }

    struct boot_record * br =  (boot_record *) ntfs;
    return &(((mft_item *) br->mft_start_address)[index]);
}

void PseudoNTFS::setClusterData(const int index, const unsigned char * data, const int size) {

    struct boot_record * br =  (boot_record *) ntfs;

    if (index < 0 || index > br->cluster_count - 1) {
        indexOutOfRange = true;
        return;
    }

    // clear cluster data
    memset(&(((unsigned char *)br->data_start_address)[index * br->cluster_size]), 0, br->cluster_size);

    // set cluster with data
    memcpy(&(((unsigned char *)br->data_start_address)[index * br->cluster_size]), data, size);
    setBitmap(index, true);
}

void PseudoNTFS::getClusterData(const int index, unsigned char * data) {
    
    struct boot_record * br =  (boot_record *) ntfs;

    if (index < 0 || index > br->cluster_count - 1) {
        indexOutOfRange = true;
        return;
    }

    memset(data, 0, br->cluster_size);
    memcpy(data, &(((unsigned char *)br->data_start_address)[index * br->cluster_size]), br->cluster_size);

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

void PseudoNTFS::save(std::list<struct data_seg> * dataSegmentList, const char * fileName, char * fileData, int32_t fileLength) {

        int8_t dataClustersCount = dataSegmentList->size();
       
        int8_t neededMftItemsCount = neededMftItems(dataClustersCount);
        if (neededMftItemsCount > freeMftItems) {
            //TODO NOT ENOUGH SPACE
            return;
        }
        
        // Prepare struct to save
        struct mft_item mftItem;
        mftItem.uid = getUID();
        mftItem.isDirectory = false;
        mftItem.item_order = 0;
        mftItem.item_order_total = mftItemsCount;
        strcpy(mftItem.item_name, fileName);
        mftItem.item_size = fileLength;

        int8_t counter, dataCounter = 0;
        int8_t mftItemsLeft = neededMftItemsCount;
        int8_t mftIndex;

        struct boot_record * br = (boot_record *) ntfs;
        for (data_seg item : *dataSegmentList) {

            counter %= MFT_FRAGMENTS_COUNT;
            
            if (counter == 0) {
                mftIndex = findFreeMft();
                clearMftItemFragments(mftItem.fragments);  
            }

            mftItem.fragments[counter].fragment_start_address = item.startIndex;
            mftItem.fragments[counter].fragment_count = item.size / br->cluster_size;
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

void PseudoNTFS::saveFileToPseudoNtfs(const char * fileName, const char * filePath) {


        std::string fileData;
        readFile(filePath, &fileData);
        // No end char - we only store values to save
        int len = fileData.length();

        char * data = new char[len];
        memcpy(data, fileData.c_str(), len);
        
        if (len > freeSpace) {
            //TODO NOT ENOUGH SPACE
            return;
        };
    
        std::list<struct data_seg> dataSegmentList;
        prepareMftItems(&dataSegmentList, len);
        save(&dataSegmentList, fileName, data, len);
    
        //TODO SET INFO TO PARENT DIRECTORY
}

void PseudoNTFS::clearMftItemFragments(mft_fragment * fragments) const {

    mft_fragment clearFragments[MFT_FRAGMENTS_COUNT] = {0, 0};
    memcpy(fragments, clearFragments, MFT_FRAGMENTS_COUNT);
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

    struct boot_record * br = (boot_record *) ntfs;

    struct mft_item * mftItem = (mft_item *) br->mft_start_address;

    for (int i = 0; i < mftItemsCount; i++) {
        if (mftItem[i].uid == UID_ITEM_FREE) {
            return i;
        }
    }

    // In case there is no free mft item
    return -1;
}

/*
demandedSize - size we need to save whole file
startIndex - index to cluster data segment
providedSize - meximal continual free space
*/
void PseudoNTFS::findFreeSpace(const int32_t demandedSize, int32_t * startIndex, int32_t * providedSize) {
    
    struct boot_record * br =  (boot_record *) ntfs;

    *providedSize = 0;
    
    int32_t maxSize = 0;
    int32_t maxIndex = 0;

    int32_t bound = ceil(br->cluster_count / 8); 
    for (int i = 0; i < bound; i++) {
        if (isClusterFree(i)) {
            if (maxSize == 0) {
                maxIndex = i;
            }
            maxSize += br->cluster_size;
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

    struct boot_record * br =  (boot_record *) ntfs;
    
    int32_t bound = size / br->cluster_size;
    int32_t left = size;
    
    for (int i = 0; i < bound; i++) {  
        setClusterData(startIndex + i, (unsigned char *) data + i * br->cluster_size , br->cluster_size);
        left -= br->cluster_size;
    }

    if (left > 0) {
        setClusterData(startIndex + bound, (unsigned char *) data + bound * br->cluster_size, left);
    }
}


int32_t PseudoNTFS::contains(const int32_t mftItemIndex, const char * name, const bool directory) {

    struct boot_record * br = (boot_record *) ntfs;
    struct mft_item * mftItem = (mft_item *) br->mft_start_address;
    
    int32_t dataClusterStartIndex, dataClustersCount, searchedMftItemIndex;
    for (int i = 0; i < MFT_FRAGMENTS_COUNT; i++) {
        dataClusterStartIndex = mftItem->fragments[i].fragment_start_address;
        dataClustersCount = mftItem->fragments[i].fragment_start_address;

        searchedMftItemIndex = searchClusters(dataClusterStartIndex, dataClustersCount, name, directory);
        if (mftItemIndex != NOT_FOUND) {
            return mftItemIndex;
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

    struct boot_record * br = (boot_record *) ntfs;

    int maxUidInDataCluster = br->cluster_size / sizeof(int32_t);
    int32_t * mftItemUid = (int32_t *) &(((unsigned char * ) br->data_start_address)[dataClusterIndex * br->cluster_size]);

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

    struct mft_item * mftItem;

    struct mft_item * tempMftItem = (mft_item *) ((boot_record *) ntfs)->mft_start_address;
    for (int i = 0; i < mftItemsCount; i++) {
        if (tempMftItem[i].uid == uid && strcmp(mftItem->item_name, name) && mftItem->isDirectory == directory) {
            return i;
        }
    }

    return NOT_FOUND;
}

/* TEST FUNCTIONS */

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