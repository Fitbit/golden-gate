package com.fitbit.bluetooth.fbgatt.rx.advertiser

import android.bluetooth.le.AdvertiseData
import android.bluetooth.le.AdvertiseSettings
import android.bluetooth.le.BluetoothLeAdvertiser
import android.content.Context
import java.util.*
import org.junit.Test
import org.junit.runner.RunWith
import org.junit.runners.JUnit4
import org.mockito.Mockito
import org.mockito.kotlin.*

@RunWith(JUnit4::class)
class AdvertiserTest {

  val mockBluetoothLeAdvertiser = Mockito.mock(BluetoothLeAdvertiser::class.java)
  val mockContext = Mockito.mock(Context::class.java)
  val advertiser = Advertiser(mockContext, mockBluetoothLeAdvertiser)

  @Test
  fun startAdvertising() {

    val advertiseSettings = mock<AdvertiseSettings>()
    val advertiseData = mock<AdvertiseData>()

    advertiser
      .startAdvertising(advertiseSettings, advertiseData, advertiseData)
      .subscribe({ println("Started advertising") }, { it.printStackTrace() })

    verify(mockBluetoothLeAdvertiser).startAdvertising(any(), any(), any(), any())
  }

  @Test
  fun stopAdvertising() {
    // var advertiseCallback: AdvertiseCallback? = null
    advertiser.stopAdvertising().test().assertComplete()
  }
}
