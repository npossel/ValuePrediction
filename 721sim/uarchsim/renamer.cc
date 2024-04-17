#include "renamer.h"

renamer::renamer(uint64_t n_log_regs, uint64_t n_phys_regs, uint64_t n_branches, 
        uint64_t n_active) {
    // Assertions for various user defined variables
    assert(n_phys_regs>n_log_regs);
    assert(1<=n_branches<=64);
    assert(n_active>0);

    //set private variables equal to the user defined variables
    phys_regs = n_phys_regs;
    log_regs = n_log_regs;
    branches = n_branches;
    active = n_active;

    // Data struc allocation and initialize
    rmt.resize(log_regs);
    std::iota (std::begin(rmt), std::end(rmt), 0);

    amt.resize(log_regs);
    std::iota (std::begin(amt), std::end(amt), 0);

    fl.resize(phys_regs-log_regs);
    std::iota (std::begin(fl), std::end(fl), log_regs);

    activelist act;
    al.resize(active, act);
    prf.resize(phys_regs);
    prfrba.resize(phys_regs,1);
    checkpointlist cpl;
    bc.resize(branches, cpl);
}

bool renamer::stall_reg(uint64_t bundle_dst) {
    // Subtract head position from tail position,  and check if 
    // there is enough room for the bundle
    uint64_t tail = fl_t;
    if(fl_tp!=fl_hp) {
        tail = tail + phys_regs - log_regs;
    }
    uint64_t free = tail - fl_h;
    if(free < bundle_dst){
        return true;
    }
    else {
        return false;
    }
}

bool renamer::stall_branch(uint64_t bundle_branch) {
    // Find the number of free spots in the checkpoint list by utilizing an
    // occupied flag. 
    uint64_t free=0;
    for(uint64_t i=0;i<branches;i++) {
        if(bc[i].occ==0) {
            free++;
        }
    }
    if(free < bundle_branch) {
        return true;
    }
    else {
        return false;
    }
}

uint64_t renamer::get_branch_mask() {
    // return the gbm

    unsigned int mask=1<<((sizeof(int)<<3)-1);
    while(mask) {
        mask >>= 1;
    }

    uint64_t gbm = GBM;
    return gbm;
}

uint64_t renamer::rename_rsrc(uint64_t log_reg) {
    // check RMT for current mapping of log reg
    uint64_t phys_src = rmt[log_reg];
    return phys_src;
}

uint64_t renamer::rename_rdst(uint64_t log_reg) {
    // check if free list is not empty. Grab phys reg at head and assign to log reg
    bool free_phys = true;
    if((fl_h==fl_t)&&(fl_hp==fl_tp)) {
        free_phys = false;
    }
    assert(free_phys==true);

    uint64_t phys_dst = fl[fl_h];
    fl_h++;
    if(fl_h==phys_regs-log_regs) {
        fl_h=0;
        fl_hp=!fl_hp;
    }

    rmt[log_reg] = phys_dst;
    return phys_dst;
}

uint64_t renamer::checkpoint() {
    // Iterate through the GBM to find a free bit. Once found, the ID is set to that
    // position and the bit is flipped. The branch checkpoint is then updated at the
    // corresponding location in the bc. 
    uint64_t ID=2000;
    for(uint64_t i=branches-1;i>=0;i--) {
        uint64_t mask = 1ULL << i;
        if(mask & GBM) {}
        else {
            ID = i;
            GBM = GBM | mask;
            break;
        }
    }
    assert(ID!=2000);
    bc[ID].smt = rmt;
    bc[ID].fl_h = fl_h;
    bc[ID].fl_hp = fl_hp;
    bc[ID].GBM = GBM;
    bc[ID].occ = 1;
    return ID;
}

bool renamer::stall_dispatch(uint64_t bundle_inst) {
    // Subtract head position from tail position,  and check if 
    // there is enough room for the bundle
    uint64_t head = al_h;
    if(al_hp==al_tp) {
        head = head + active;
    }
    uint64_t free = head - al_t;
    if(free < bundle_inst){
        return true;
    }
    else {
        return false;
    }
}

uint64_t renamer::dispatch_inst(bool dest_valid, uint64_t log_reg, 
                            uint64_t phys_reg, bool load, bool store, bool branch,
                            bool amo, bool csr, uint64_t PC) {
    // Check if the active list is currently full or not
    bool free_entry = true;
    if((al_h==al_t)&&(al_hp!=al_tp)) {
        free_entry = false;
    }
    assert(free_entry==true);

    // If the destination is valid, update all bits in al entry
    if(dest_valid==true) {
        al[al_t].dst_f=1;
        al[al_t].log_reg=log_reg;
        al[al_t].phys_reg=phys_reg;
    }
    else {
        al[al_t].dst_f=0;
    }
    // Update all bits in al entry
    al[al_t].cmp=0;
    al[al_t].except=0;
    al[al_t].load_vtn=0;
    al[al_t].br_mis=0;
    al[al_t].val_mis=0;
    al[al_t].ld_f=load;
    al[al_t].st_f=store;
    al[al_t].br_f=branch;
    al[al_t].amo_f=amo;
    al[al_t].csr_f=csr;
    al[al_t].cnt=PC;
    al[al_t].occ=1;

    // Save current AL index and then update AL tail
    uint64_t inst_ind = al_t;
    al_t++;
    if(al_t==active) {
        al_t=0;
        al_tp=!al_tp;
    }

    return inst_ind;
}

bool renamer::is_ready(uint64_t phys_reg) {
    // Check if the associated phys reg is ready (bit set to 1)
    if(prfrba[phys_reg]==1) {
        return true;
    }
    else {
        return false;
    }
}

void renamer::clear_ready(uint64_t phys_reg) {
    // Clear the associated read bit of the phys reg (set to 0)
    prfrba[phys_reg]=0;
}

uint64_t renamer::read(uint64_t phys_reg) {
    // Read contents of the given phys reg
    return prf[phys_reg];
}

void renamer::set_ready(uint64_t phys_reg) {
    // Set the associated ready bit of the phys reg (set to 1)
    prfrba[phys_reg]=1;
}

void renamer::write(uint64_t phys_reg, uint64_t value) {
    // Write given value to given phys reg
    prf[phys_reg]=value;
}

void renamer::set_complete(uint64_t AL_index) {
    // Set the completed bit of given AL index to 1
    al[AL_index].cmp=1;
}

void renamer::resolve(uint64_t AL_index, uint64_t branch_ID, bool correct) {
    // Clear branch's bit in GBM to 0
    uint64_t mask;
    mask = ~(1ULL << branch_ID);
    if(correct) {
        GBM = GBM & mask;

        // Clear branch's bit in GBM of all checkpointed branches
        for(uint64_t i=0; i<branches; i++) {
            if(bc[i].occ==1) {
                bc[i].GBM = bc[i].GBM & mask;
            }
        }
        // Clear occupied position in bc
        bc[branch_ID].occ=0;
    }
    else {
    // Restore and clear GBM
        unsigned int maskx=1<<((sizeof(int)<<3)-1);

        GBM = bc[branch_ID].GBM;
        GBM = GBM & mask;
        for(uint64_t i=0; i<branches; i++) {
            if(bc[i].occ==1) {
                bc[i].GBM = bc[i].GBM & mask;
            }
        }
        maskx=1<<((sizeof(int)<<3)-1);
        while(maskx) {
            maskx >>= 1;
        }
    // Restore RMT
        rmt = bc[branch_ID].smt;
    // Restore free list head and phase
        fl_h = bc[branch_ID].fl_h;
        fl_hp = bc[branch_ID].fl_hp;
    // Restore active list tail and phase to entry just after mispredicted branch
        al_t = AL_index+1;
        if(al_t==active) {
            al_t=0;
        }
        if(al_t<=al_h) {
            al_tp=!al_hp;
        }
        else {
            al_tp=al_hp;
        }
        int array[branches];
        for(int i=0; i<branches; i++) {
            array[i] = (GBM >> i) & 1;
        }
        for(int i=0; i<branches; i++) {
            if(array[i]==0) {
                bc[i].occ=0;
            }
        }
    }
}

bool renamer::precommit(bool &completed,
                    bool &exception, bool &load_viol, bool &br_misp, bool &val_misp,
                    bool &load, bool &store, bool &branch, bool &amo, bool &csr, uint64_t &offending_PC) {
    // Set all arguments to corresponding values at the head of the active list
    // Return true if active list head has a value, otherwise return false;
    bool empt;
    if(al_h==al_t && al_hp==al_tp) { empt=true; }
    else { empt=false; }
    if(al[al_h].occ==1 && empt==false) {
        completed = al[al_h].cmp;
        exception = al[al_h].except;
        load_viol = al[al_h].load_vtn;
        br_misp = al[al_h].br_mis;
        val_misp = al[al_h].val_mis;
        load = al[al_h].ld_f;
        store = al[al_h].st_f;
        branch = al[al_h].br_f;
        amo = al[al_h].amo_f;
        csr = al[al_h].csr_f;
        offending_PC = al[al_h].cnt;
        return true;
    }
    else {
        return false;
    }
}

void renamer::commit() {
    // Go through all necessary assertions:
    // - there is a head instruction
	// - the head instruction is completed
	// - the head instruction is not marked as an exception
	// - the head instruction is not marked as a load violation
    assert(al[al_h].occ==1);
    assert(al[al_h].cmp==1);
    assert(al[al_h].except!=1);
    assert(al[al_h].load_vtn!=1);

    if(al[al_h].dst_f) {
        fl[fl_t] = amt[al[al_h].log_reg];
        fl_t++;
        if(fl_t==(phys_regs-log_regs)) {
            fl_t = 0;
            fl_tp = !fl_tp;
        }  
        amt[al[al_h].log_reg] = al[al_h].phys_reg;
    }
    al[al_h].occ=0;
    al_h++;
    if(al_h==active) {
        al_h = 0;
        al_hp = !al_hp;
    }
}

void renamer::squash() {
    // Flush active list
    al_t = al_h;
    al_tp = al_hp;

    al.clear();
    activelist act;
    al.resize(active, act);

    // restore free list
    fl_h = fl_t;
    fl_hp = !fl_tp;

    // restore rmt from amt
    rmt = amt;

    bc.clear();
    checkpointlist cpl;
    bc.resize(branches, cpl);
    GBM=0;
}

void renamer::set_exception(uint64_t AL_index) {
    al[AL_index].except = 1;
}

void renamer::set_load_violation(uint64_t AL_index) {
    al[AL_index].load_vtn = 1;
}

void renamer::set_branch_misprediction(uint64_t AL_index) {
    al[AL_index].br_mis = 1;
}

void renamer::set_value_misprediction(uint64_t AL_index) {
    al[AL_index].val_mis = 1;
}

bool renamer::get_exception(uint64_t AL_index) {
    return al[AL_index].except;
}