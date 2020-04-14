package com.fitbit.goldengate.bt.gatt.client.services

import com.fitbit.bluetooth.fbgatt.GattConnection
import com.fitbit.bluetooth.fbgatt.rx.client.BitGattPeripheral
import com.fitbit.bluetooth.fbgatt.rx.client.GattCharacteristicReader
import com.fitbit.bluetooth.fbgatt.rx.client.GattCharacteristicWriter
import com.fitbit.goldengate.bt.gatt.client.services.GattDatabaseConfirmationService.EphemeralCharacteristicPointer
import com.fitbit.goldengate.bt.gatt.util.toUuid
import io.reactivex.Completable
import io.reactivex.Single
import timber.log.Timber
import java.util.UUID

/**
 * The class implements the Gatt database version validation
 */
class GattDatabaseValidator(
    private val gattCharacteristicReaderProvider: (GattConnection) -> GattCharacteristicReader =  { GattCharacteristicReader(it) },
    private val gattCharacteristicWriterProvider: (GattConnection) -> GattCharacteristicWriter =  { GattCharacteristicWriter(it) }
) {
    /**
     * To validate the GATT Database version, first read the payload of Ephemeral Pointer Characteristic from [GattDatabaseConfirmationService]
     * The payload will contains the UUID of GATT Ephemeral Characteristic. Second, we write to Ephemeral GATT Characteristic UUID.
     * If the write succeeds within timeout, tracker will not send us the service changed indication and complete the validation process.
     */
    fun validate(peripheral: BitGattPeripheral): Completable {
        return readEphemeralPointerCharacteristic(peripheral)
            .flatMapCompletable { ephemeralCharacteristicUuid -> writeEphemeralCharacteristic(peripheral, ephemeralCharacteristicUuid) }
            .onErrorResumeNext { t -> Completable.error(GattDatabaseValidatorException("Failed to validate GATT database", t)) }
    }

    private fun readEphemeralPointerCharacteristic(peripheral: BitGattPeripheral): Single<UUID> {
        return gattCharacteristicReaderProvider(peripheral.gattConnection)
            .read(GattDatabaseConfirmationService.uuid, GattDatabaseConfirmationService.EphemeralCharacteristicPointer.uuid)
            .doOnSubscribe { Timber.d("Reading Gatt Database Ephemeral Characteristic ${EphemeralCharacteristicPointer.uuid}") }
            .doOnSuccess { Timber.d("Successfully read ${EphemeralCharacteristicPointer.uuid}") }
            .doOnError { t -> Timber.w(t, "Failed to read ${EphemeralCharacteristicPointer.uuid}") }
            .map { data -> data.toUuid() }
    }

    private fun writeEphemeralCharacteristic(peripheral: BitGattPeripheral, ephemeralCharacteristicUuid: UUID): Completable {
        return gattCharacteristicWriterProvider(peripheral.gattConnection)
            .write(GattDatabaseConfirmationService.uuid, ephemeralCharacteristicUuid, byteArrayOf(0))
            .doOnSubscribe { Timber.d("Writing to Gatt Database Ephemeral Characteristic $ephemeralCharacteristicUuid") }
            .doOnComplete { Timber.d("Successfully write Gatt Database Ephemeral Characteristic $ephemeralCharacteristicUuid") }
            .doOnError { t -> Timber.w(t, "Failed to write Gatt Database Ephemeral Characteristic $ephemeralCharacteristicUuid") }
    }
}

class GattDatabaseValidatorException(message: String? = null, throwable: Throwable? = null): Exception(message, throwable)
