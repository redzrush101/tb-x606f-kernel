#!/usr/bin/env python3
"""Run the actual compatibility functions with mocked firmware and clock ticks."""
from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]

def function(path, signature):
    text = (root / path).read_text()
    start = text.index(signature)
    brace = text.index('{', start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]

mpu = function('drivers/misc/mediatek/emi/submodule/mpu_v1.c', 'static void emi_mpu_call(')
wait = function('drivers/misc/mediatek/scp/cm4/scp_dvfs.c', 'int wait_scp_dvfs_init_done(')
constants = '\n'.join(line for line in (root / 'include/memory/mediatek/emi.h').read_text().splitlines()
                      if re.match(r'#define MTK_EMIMPU_', line))
registers = '\n'.join(line for line in (root / 'drivers/misc/mediatek/emi/mt6765/mt_emi.h').read_text().splitlines()
                     if line.startswith('#define EMI_MPU_'))
# The APC definition is continued on the following line.
registers = registers.replace('#define EMI_MPU_APC(region, dgroup) \\',
                              '#define EMI_MPU_APC(region, dgroup) (0x300 + (region)*4 + (dgroup)*0x100)')
source = r'''
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
struct arm_smccc_res { unsigned long a0, a1, a2, a3; };
static int legacy = 1, calls, errors, complete_at;
static void *of_root;
static unsigned long args[8], jiffies;
static int g_scp_dvfs_init_flag = -1;
#define MTK_SIP_SMC_CMD(n) (0xc2000000UL | (n))
#define MTK_SIP_EMIMPU_CONTROL MTK_SIP_SMC_CMD(0x50b)
#define SIP_SVC_E_INVALID_PARAMS -2
#define SIP_SVC_E_NOT_SUPPORTED -1
#define READ_ONCE(x) (x)
#define msecs_to_jiffies(x) (x)
#define time_after_eq(a,b) ((long)((a)-(b)) >= 0)
#define pr_err(...) (++errors)
static int of_property_read_bool(void *node, const char *key) {
    assert(node == of_root && !strcmp(key, "mediatek,legacy-emimpu"));
    return legacy;
}
static void msleep(unsigned int n) {
    jiffies += n;
    if (complete_at && jiffies >= (unsigned int)complete_at) g_scp_dvfs_init_flag = 1;
}
static void arm_smccc_smc(unsigned long id, unsigned long a, unsigned long b,
    unsigned long c, unsigned long d, unsigned long e, unsigned long f,
    unsigned long g, struct arm_smccc_res *res) {
    calls++;
    args[0]=id; args[1]=a; args[2]=b; args[3]=c;
    args[4]=d; args[5]=e; args[6]=f; args[7]=g;
    res->a0 = 0x21;
}
'''
source += constants + '\n' + registers + '\n' + mpu + '\n' + wait + r'''
int main(void) {
    struct arm_smccc_res res;
    emi_mpu_call(MTK_EMIMPU_SET, 0x60123, 0x10456, 0x80000000, &res);
    assert(args[0]==0xc2000262 && args[1]==0x60123 && args[2]==0x10456 && args[3]==0x80000000);
    assert(args[4]==0 && args[5]==0 && args[6]==0 && args[7]==0);
    emi_mpu_call(MTK_EMIMPU_CLEAR, 6, 0, 0, &res);
    assert(args[0]==0xc2000263 && args[1]==6 && args[2]==0);
    emi_mpu_call(MTK_EMIMPU_READ, MTK_EMIMPU_READ_SA, 6, 0, &res);
    assert(args[0]==0xc2000261 && args[1]==0x118 && res.a0==0x21);
    emi_mpu_call(MTK_EMIMPU_READ, MTK_EMIMPU_READ_EA, 6, 0, &res);
    assert(args[1]==0x218);
    emi_mpu_call(MTK_EMIMPU_READ, MTK_EMIMPU_READ_APC, 6, 1, &res);
    assert(args[1]==0x418);
    int before=calls;
    emi_mpu_call(MTK_EMIMPU_READ, 99, 0, 0, &res);
    assert(calls==before && (long)res.a0==-2);
    emi_mpu_call(MTK_EMIMPU_SLVERR, 3, 0, 0, &res);
    assert(calls==before+2 && args[0]==0xc2000260 && args[1]==0x80c && args[2]==0x23);
    legacy=0;
    emi_mpu_call(MTK_EMIMPU_SET, 11, 22, 33, &res);
    assert(args[0]==0xc200050b && args[1]==0 && args[2]==11 && args[3]==22 && args[4]==33);
    complete_at=5;
    assert(wait_scp_dvfs_init_done()==0 && jiffies==5 && errors==0);
    complete_at=0; g_scp_dvfs_init_flag=-1; jiffies=0;
    assert(wait_scp_dvfs_init_done()==-ETIMEDOUT && jiffies==3000 && errors==1);
    puts("PASS: legacy MPU argument packing, modern fallback, SCP success and timeout");
}
'''
with tempfile.TemporaryDirectory() as directory:
    c = Path(directory) / 'compat.c'
    binary = Path(directory) / 'compat'
    c.write_text(source)
    subprocess.run(['gcc', '-std=gnu11', '-Wall', '-Wextra', '-Werror', str(c), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
