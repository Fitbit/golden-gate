package com.fitbit.goldengate.bt.gatt.server.services.gattcache

import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import com.fitbit.bluetooth.fbgatt.FitbitBluetoothDevice
import com.fitbit.bluetooth.fbgatt.GattServerConnection
import com.fitbit.bluetooth.fbgatt.TransactionResult
import com.fitbit.bluetooth.fbgatt.rx.server.GattServerResponseSender
import com.fitbit.bluetooth.fbgatt.rx.server.GattServerResponseSenderProvider
import com.fitbit.goldengate.bt.gatt.util.toByteArray
import io.reactivex.Completable
import io.reactivex.schedulers.Schedulers
import java.util.UUID
import org.junit.After
import org.junit.Before
import org.junit.Test
import org.mockito.kotlin.any
import org.mockito.kotlin.anyOrNull
import org.mockito.kotlin.doReturn
import org.mockito.kotlin.mock
import org.mockito.kotlin.never
import org.mockito.kotlin.verify
import org.mockito.kotlin.whenever

class GattCacheServiceEventListenerTest {

  private val device1 = mock<BluetoothDevice> { on { address } doReturn "1" }
  private val unknownUuid = UUID.fromString("FFFFFFFF-FFFF-FFFF-FFFF-FFFFFFFFFFFF")

  private val mockGattServerConnection = mock<GattServerConnection>()
  private val mockGattServerResponseSender = mock<GattServerResponseSender>()

  private val mockGattServerResponseSenderProvider =
    mock<GattServerResponseSenderProvider> {
      on { provide(mockGattServerConnection) } doReturn mockGattServerResponseSender
    }

  private val listener =
    GattCacheServiceEventListener(Schedulers.trampoline(), mockGattServerResponseSenderProvider)

  private fun mockGattServerResponse() {
    whenever(mockGattServerResponseSender.send(any(), any(), any(), any(), any(), anyOrNull()))
      .thenReturn(Completable.complete())
  }

  @Before
  fun setUp() {
    mockGattServerResponse()
  }

  @After fun tearDown() {}

  @Test
  fun `Read EphemeralCharacteristicPointer successfully`() {
    listener.onServerCharacteristicReadRequest(
      device1,
      mockTransactionResult(characteristicId = EphemeralCharacteristicPointer.uuid),
      mockGattServerConnection
    )
    verifyResponseSent(value = EphemeralCharacteristic.uuid.toByteArray())
  }

  @Test
  fun `Read request to undefined service UUID`() {
    listener.onServerCharacteristicReadRequest(
      device1,
      mockTransactionResult(
        serviceId = unknownUuid,
        characteristicId = EphemeralCharacteristicPointer.uuid
      ),
      mockGattServerConnection
    )

    verifyNoResponseSent()
  }

  @Test
  fun `Read request to undefined characteristic UUID`() {
    listener.onServerCharacteristicReadRequest(
      device1,
      mockTransactionResult(characteristicId = unknownUuid),
      mockGattServerConnection
    )

    verifyFailureSent()
  }

  @Test
  fun `Write EphemeralCharacteristic successfully`() {
    listener.onServerCharacteristicWriteRequest(
      device1,
      mockTransactionResult(
        characteristicId = EphemeralCharacteristic.uuid,
        value = byteArrayOf(0)
      ),
      mockGattServerConnection
    )
    verifyResponseSent(BluetoothGatt.GATT_SUCCESS)
  }

  @Test
  fun `Write request to undefined service UUID`() {
    listener.onServerCharacteristicWriteRequest(
      device1,
      mockTransactionResult(
        serviceId = unknownUuid,
        characteristicId = EphemeralCharacteristic.uuid,
        value = byteArrayOf(1)
      ),
      mockGattServerConnection
    )
    verifyNoResponseSent()
  }

  @Test
  fun `Write request to undefined characteristic UUID`() {
    listener.onServerCharacteristicWriteRequest(
      device1,
      mockTransactionResult(characteristicId = unknownUuid, value = byteArrayOf(1)),
      mockGattServerConnection
    )
    verifyFailureSent()
  }

  @Test
  fun `Write EphemeralCharacteristic with wrong data`() {
    listener.onServerCharacteristicWriteRequest(
      device1,
      mockTransactionResult(
        characteristicId = EphemeralCharacteristic.uuid,
        value = byteArrayOf(1)
      ),
      mockGattServerConnection
    )
    verifyFailureSent()
  }

  private fun mockTransactionResult(
    serviceId: UUID = GattCacheService.uuid,
    characteristicId: UUID,
    responseNeeded: Boolean = true,
    value: ByteArray? = null
  ): TransactionResult {
    return mock {
      on { requestId } doReturn 1
      on { serviceUuid } doReturn serviceId
      on { characteristicUuid } doReturn characteristicId
      on { isResponseRequired } doReturn responseNeeded
      on { data } doReturn value
    }
  }

  private fun verifyResponseSent(
    status: Int = BluetoothGatt.GATT_SUCCESS,
    value: ByteArray? = null
  ) {
    verify(mockGattServerResponseSender)
      .send(device = FitbitBluetoothDevice(device1), requestId = 1, status = status, value = value)
  }

  private fun verifyFailureSent() {
    verify(mockGattServerResponseSender)
      .send(
        device = FitbitBluetoothDevice(device1),
        requestId = 1,
        status = BluetoothGatt.GATT_FAILURE
      )
  }

  private fun verifyNoResponseSent() {
    verify(mockGattServerResponseSender, never())
      .send(any(), any(), any(), any(), any(), anyOrNull())
  }
}
