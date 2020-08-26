package com.fitbit.goldengate.bindings.util

import org.junit.Assert
import org.junit.Test

class IntExtKtTest {
    @Test
    fun `can decode block info`() {
        /**
         * test cases for 1-byte format
         *  0
         *  0 1 2 3 4 5 6 7
         * +-+-+-+-+-+-+-+-+
         * |  NUM  |M| SZX |
         * +-+-+-+-+-+-+-+-+
         */
        val data1 = 0x0e
        var blockInfo = data1.toBlockInfo()
        Assert.assertEquals(blockInfo.moreBlock, true)
        Assert.assertEquals(blockInfo.startOffset, 0)

        val data2 = 0xf6
        blockInfo = data2.toBlockInfo()
        Assert.assertEquals(blockInfo.moreBlock, false)
        Assert.assertEquals(blockInfo.startOffset, 1024 * 15)

        /**
         * test cases for 2-byte format
         *   0                   1
         *   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
         *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *  |          NUM          |M| SZX |
         *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         */
        val data3 = 0x012e
        blockInfo = data3.toBlockInfo()
        Assert.assertEquals(blockInfo.moreBlock, true)
        Assert.assertEquals(blockInfo.startOffset, 1024 * 18)

        val data4 = 0x0276
        blockInfo = data4.toBlockInfo()
        Assert.assertEquals(blockInfo.moreBlock, false)
        Assert.assertEquals(blockInfo.startOffset, 1024 * 39)

        val data5 = 0xf41e
        blockInfo = data5.toBlockInfo()
        Assert.assertEquals(blockInfo.moreBlock, true)
        Assert.assertEquals(blockInfo.startOffset, 1024 * 3905)

        val data6 = 0xf426
        blockInfo = data6.toBlockInfo()
        Assert.assertEquals(blockInfo.moreBlock, false)
        Assert.assertEquals(blockInfo.startOffset, 1024 * 3906)

        /**
         * test cases for 3-byte format
         *  0                   1                   2
         *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3
         * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         * |                   NUM                 |M| SZX |
         * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         */
        val data7 = 0x0166de
        blockInfo = data7.toBlockInfo()
        Assert.assertEquals(blockInfo.moreBlock, true)
        Assert.assertEquals(blockInfo.startOffset, 1024 * 5741)

        val data8 = 0x016e36
        blockInfo = data8.toBlockInfo()
        Assert.assertEquals(blockInfo.moreBlock, false)
        Assert.assertEquals(blockInfo.startOffset, 1024 * 5859)

        val data9 = 0xfffffe
        blockInfo = data9.toBlockInfo()
        Assert.assertEquals(blockInfo.moreBlock, true)
        Assert.assertEquals(blockInfo.startOffset, 1024 * 1048575)

        val data10 = 0xfffff6
        blockInfo = data10.toBlockInfo()
        Assert.assertEquals(blockInfo.moreBlock, false)
        Assert.assertEquals(blockInfo.startOffset, 1024 * 1048575)
    }
}
