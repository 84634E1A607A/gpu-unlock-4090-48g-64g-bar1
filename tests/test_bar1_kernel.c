/* SPDX-License-Identifier: GPL-2.0-only */
#define main ga102_fixture_main
#include "test_unlock_adapter.c"
#undef main

static void bar1_setup(struct unlock_transaction_work *w)
{
    setup(w);bar1_64g=true;fake.bar1_payload=1;
    dev.device=GU_AD102_RTX_4090;w->bar1_64g=true;
    regs[0]=w->chip_entry=0x192000a1;
    regs[GU_XVE_PLM/4]=0xffffff8f;
    regs[GU_XVE_CYA/4]=0x28;regs[GU_XVE_CFG/4]=0x80000009;
    regs[GU_XVE_BAR1_CAP/4]=0xffc00;
    regs[0x1fa7cc/4]=regs[0x1fa7c4/4]=0x0004cb8f;
    regs[GU_FEAT_PLM_OFFSET/4]=0xffffff8f;
    regs[GU_FEAT_SS0_OFFSET/4]=0x64023025;regs[GU_FEAT_SS1_OFFSET/4]=7;
    regs[GU_FEAT_READOUT_1_OFFSET/4]=0x410080;
    regs[GU_GFW_PLM_OFFSET/4]=1;regs[GU_GFW_BOOT_OFFSET/4]=0xff;
    regs[GU_GFW_CPUCTL_OFFSET/4]=0x10;
}

int main(int argc,char **argv)
{
    struct unlock_transaction_work w;
    assert(bar1_64g);
    /* Host verification never retries private writes, including after FLR. */
    for(int mode=0;mode<=8;mode++) {
        bar1_setup(&w);fake.bar1_mode=mode;
        w.payload_verified=w.sec2_halt_verified=true;
        regs[GU_XVE_PLM/4]=~0u;
        regs[GU_XVE_CYA/4]=mode==2?0x28:0x30;
        regs[GU_XVE_CFG/4]=mode==3?0x80000009:0x8000000a;
        regs[GU_XVE_BAR1_CAP/4]=mode==4?0xffc00:0x1ffc00;
        int ret=unlock_transaction_bar1(&w,false);
        assert((!ret)==(mode!=2 && mode!=3 && mode!=4 && mode!=5 && mode!=8));
        assert(!fake.writes);
    }
    bar1_setup(&w);w.bar1_64g=false;
    assert(!unlock_transaction_bar1(&w,false) && !fake.writes);
    bar1_setup(&w);
    assert(unlock_transaction_bar1(&w,false)==-EPROTO && !fake.writes);
    /* Fixed secure-write arguments reject unknown reserved bits beforehand. */
    for(int mode=0;mode<2;mode++) {
        bar1_setup(&w);
        if(mode)regs[GU_XVE_CYA/4]|=0x100;
        else regs[GU_XVE_CFG/4]|=0x4000000;
        assert(unlock_transaction_gfw(&w)==-EOPNOTSUPP);
        assert(!fake.writes && !fake.starts && !fake.gsp_starts);
    }
    if(argc==3) {
        asset_root=argv[2];full_active=true;
        for(int mode=0;mode<=9;mode++) {
            bar1_setup(&w);fake.bar1_mode=mode;load_rom(argv[1]);
            regs[0x1183a4/4]=0x5ff4;regs[0x625f04/4]=1;regs[0x1fa7c8/4]=0x45cb8f;
            regs[0x1fa82c/4]=0x100000;regs[0x1fa830/4]=0x5cf3ff0;
            regs[(GU_FUSE_SEC2_UCODE1+2*4)/4]=1;
            fake.fwsec_wpr=1;fake.fwsec_wpr_lo=0x5ff2000;fake.fwsec_wpr_hi=0x5ff2e00;
            struct gpu_unlock_610_shared shared={0};last_work=NULL;
            assert(!gpu_unlock_610_load_shared(&dev,&shared));
            int ret=gpu_unlock_610_unlock(&dev,&shared);
            gpu_unlock_610_release_shared(&shared);
            bool success=mode==0;
            if((!ret)!=success)fprintf(stderr,"%s\nBAR1 full mode=%d ret=%d\n",fake.log,mode,ret);
            assert((!ret)==success && fake.starts==1 && fake.gsp_starts==1);
            if(mode==1 || mode==2 || mode==3 || mode==9)assert(!fake.selector_writes && !fake.flrs);
            else assert(fake.flrs==1);
            if(success)assert(regs[GU_XVE_BAR1_CAP/4]==0x1ffc00);
            if(fake.pins) {
                assert(last_work->retained && !last_work->quiesced);
                last_work->firmware_started=false;
                assert(!unlock_transaction_unwind(last_work));
                pci_dev_put(last_work->pdev);free(last_work);
            }
            assert(!fake.live && !fake.fw_live && dev.refs==1 && !fake.regions);
            printf("BAR1 full mode=%d result=%d PASS\n",mode,ret);
        }
    }
    puts("BAR1 direct payload, capability, reserved bits, FLR and cleanup gates PASS under mocked hardware");
}
