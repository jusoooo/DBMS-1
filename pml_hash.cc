#include "pml_hash.h"
/**
 * PMLHash::PMLHash 
 * 
 * @param  {char*} file_path : the file path of data file
 * if the data file exist, open it and recover the hash
 * if the data file does not exist, create it and initial the hash
 */
PMLHash::PMLHash(const char* file_path) {
    if((start_addr=pmem_map_file(file_path,FILE_SIZE,PMEM_FILE_CREATE,0666,&mapped_len,&is_pmem))==NULL){
        cout<<"faild to map file."<<endl;
    }
    cout<<start_addr<<endl;
    metadata * meta = (metadata*)start_addr;
    meta->level=1;
    meta->next=0;
    meta->overflow_num=0;
    meta->size=2;
    N=(int)pow(2,meta->level);
    pmem_persist(meta,meta_size);

    table_addr=(pm_table*)((metadata*)start_addr+1);
    table_addr->fill_num=0;
    table_addr->next_offset=0;

    pm_table*table_addr2=table_addr+1;
    table_addr2->fill_num=0;
    table_addr2->next_offset=0;
    overflow_addr=(pm_table*)((char*)start_addr+1024*1024*8);
}
/**
 * PMLHash::~PMLHash 
 * 
 * unmap and close the data file
 */
PMLHash::~PMLHash() {
    pmem_unmap(start_addr, FILE_SIZE);
}
/**
 * PMLHash 
 * 
 * split the hash table indexed by the meta->next
 * update the metadata
 */
void PMLHash::split() { 
    uint64_t mod_num=(int)pow(2,meta->level+1);
    pm_table*new_table=table_addr+meta->size;
    new_table->fill_num=0;
    new_table->next_offset=0;
    meta->size+=1;
    meta->next+=1;
    //想办法更新到新表
    pm_table * pre_table = table_addr+meta->next;
    

    if(meta->next==N){
        meta->level+=1;
        meta->next=0;
        N*=2;
    }
    // fill the split table

    // fill the new table

    // update the next of metadata

}
/**
 * PMLHash 
 * 
 * merge the hash table indexed by the meta->next
 * update the metadata
 */
void PMLHash::merge() {
    if(meta->size == 2){
        return;
    }

    // update the next of metadata
    if(meta->next != 0){
        meta->next -= 1;
    }
    else{
        N /= 2;
        meta->next = N-1;
    }meta->size -= 1;

    pm_table*idx_table1 = table_addr+meta->next;
    pm_table*idx_table2 = idx_table1+N;

    while(idx_table1->next_offset != 0){
        idx_table1 = (pm_table *)overflow_addr+idx_table1->next_offset-1;
    }
    int i = 0;int j = idx_table1->fill_num;
    pm_table*new_table;
    while(i < idx_table2->fill_num || idx_table2->next_offset != 0){
        if(j == TABLE_SIZE){
            new_table = newOverflowTable(meta->overflow_num);
            idx_table1->next_offset = meta->overflow_num;
            idx_table1 = new_table;
            j = 0;
        }
        idx_table1->kv_arr[j].key = idx_table2->kv_arr[i].key;
        idx_table1->kv_arr[j].value = idx_table2->kv_arr[i].value;
        ++i;++j;
        if(i == TABLE_SIZE && idx_table2->next_offset != 0){
            idx_table2 = (pm_table *)overflow_addr+idx_table2->next_offset-1;
        }
    }
}
/**
 * PMLHash 
 * 
 * @param  {uint64_t} key     : key
 * @param  {size_t} hash_size : the N in hash func: idx = hash % N
 * @return {uint64_t}         : index of hash table array
 * 
 * need to hash the key with proper hash function first
 * then calculate the index by N module
 */
uint64_t PMLHash::hashFunc(const uint64_t &key, const size_t &hash_size) {
    return key%N;
}

/**
 * PMLHash 
 * 
 * @param  {uint64_t} offset : the file address offset of the overflow hash table
 *                             to the start of the whole file
 * @return {pm_table*}       : the virtual address of new overflow hash table
 */
pm_table* PMLHash::newOverflowTable(uint64_t offset) {
    pm_table*idx_overflow_table=NULL;
    while(offset){
        idx_overflow_table=(pm_table*)overflow_addr+offset;
        offset=idx_overflow_table->next_offset;
    }
    int num=idx_overflow_table->fill_num;
    if(num<TABLE_SIZE)
        return idx_overflow_table;
    else{
        meta->overflow_num+=1;
        idx_overflow_table->next_offset=meta->overflow_num;
        idx_overflow_table=(pm_table*)overflow_addr+offset;
        idx_overflow_table->fill_num=0;
        idx_overflow_table->next_offset=0;
        return idx_overflow_table;
    }
}


/**
 * PMLHash 
 * 
 * @param  {uint64_t} key   : inserted key
 * @param  {uint64_t} value : inserted value
 * @return {int}            : success: 0. fail: -1
 * 
 * insert the new kv pair in the hash
 * 
 * always insert the entry in the first empty slot
 * 
 * if the hash table is full then split is triggered
 */
int PMLHash::insert(const uint64_t &key, const uint64_t &value) {
    uint64_t idx=hashFunc(key,N);
    pm_table*idx_table=table_addr+idx;
    uint64_t num=idx_table->fill_num;
    uint64_t offset=idx_table->next_offset;
    if(num<TABLE_SIZE){
        entry*entry_addr=idx_table->kv_arr;
        entry_addr=entry_addr+num;
        entry_addr->key=key;
        entry_addr->value=value;
        idx_table->fill_num+=1;
    }else{
        idx_table=newOverflowTable(offset);
        num=idx_table->fill_num;
        entry*entry_addr=(entry*)idx_table;
        entry_addr=entry_addr+num;
        entry_addr->key=key;
        entry_addr->value=value;
        idx_table->fill_num+=1;
        split();
    }
    return 0;
}

/**
 * PMLHash 
 * 
 * @param  {uint64_t} key   : the searched key
 * @param  {uint64_t} value : return value if found
 * @return {int}            : 0 found, -1 not found
 * 
 * search the target entry and return the value
 */
int PMLHash::search(const uint64_t &key, uint64_t &value) {
    return 0;
}

/**
 * PMLHash 
 * 
 * @param  {uint64_t} key : target key
 * @return {int}          : success: 0. fail: -1
 * 
 * remove the target entry, move entries after forward
 * if the overflow table is empty, remove it from hash
 */
int PMLHash::remove(const uint64_t &key) {
    uint64_t idx=hashFunc(key,N);
    pm_table*idx_table=table_addr+idx;

    pm_table*now_table=idx_table;
    pm_table*pre_table=nullptr;   //the pre of last_table
    pm_table*last_table=idx_table;//the last of overflowpage

    //calculate the total count of element
    uint64_t num=idx_table->fill_num;
    while(last_table->next_offset == 0){
        pre_table = last_table;
        last_table = (pm_table *)overflow_addr+idx_table->next_offset-1;
        num += last_table->fill_num;
    }
    int ans = -1;
    for(int i = 0;i < num;++i){
        if(now_table->kv_arr[i%TABLE_SIZE].key == key){
            ans = now_table->kv_arr[i%TABLE_SIZE].value;
            now_table->kv_arr[i%TABLE_SIZE].key = last_table->kv_arr[(num-1)%TABLE_SIZE].key;
            now_table->kv_arr[i%TABLE_SIZE].value = last_table->kv_arr[(num-1)%TABLE_SIZE].value;
            last_table->fill_num -= 1;
            if(last_table->fill_num == 0){
                if(pre_table){
                    pre_table->next_offset = 0;
                }else{
                    merge();
                }
            }break;
        }
        if((i+1)%TABLE_SIZE == 0){
            now_table = (pm_table *)overflow_addr+now_table->next_offset;
        }
    }
    return ans;
}

/**
 * PMLHash 
 * 
 * @param  {uint64_t} key   : target key
 * @param  {uint64_t} value : new value
 * @return {int}            : success: 0. fail: -1
 * 
 * update an existing entry
 */
int PMLHash::update(const uint64_t &key, const uint64_t &value) {
    return 0;
}

void PMLHash::show(){

}