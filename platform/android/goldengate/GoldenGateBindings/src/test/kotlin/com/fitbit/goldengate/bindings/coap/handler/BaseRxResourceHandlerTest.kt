package com.fitbit.goldengate.bindings.coap.handler

import io.reactivex.schedulers.TestScheduler
import org.junit.Assert.assertTrue
import org.junit.Test
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicBoolean

class BaseRxResourceHandlerTest {

    // The class we're testing is abstract, so we need a concrete subclass to instantiate it
    inner class TestRxResourceHander: BaseRxResourceHandler() {
        fun emit(data: ByteArray) {
            emitUpdates(data)
        }
    }

    @Test
    fun `updates should be emitted on given scheduler`() {
        val sut = TestRxResourceHander()
        val scheduler = TestScheduler()
        val testValue = byteArrayOf(1, 2, 3)
        val observer = sut.observeUpdates(scheduler).test()
        sut.emit(testValue)
        observer.assertNoValues()
        scheduler.triggerActions()
        observer.assertValue(testValue)
        //This test isn't 100% reliable and could pass spuriously if whatever wrong scheduler is used
        //happens to coincidentally execute the onNext at the same time as we call triggerActions()
    }

    @Test
    fun `with no scheduler passed, updates do not block the thread that calls emitUpdates`() {
        val sut = TestRxResourceHander()
        val testValue = byteArrayOf(1, 2, 3)
        val completedWithoutBlocking = AtomicBoolean(false)
        val blockingLatch = CountDownLatch(1)
        val resultLatch = CountDownLatch(1)
        val observer = sut.observeUpdates()
            .doOnNext {
                //Block the thread that emits
                completedWithoutBlocking.set(blockingLatch.await(50, TimeUnit.MILLISECONDS))
                resultLatch.countDown()
            }
            .test()
        sut.emit(testValue)
        observer.assertNoValues() //No values yet because the blockingLatch prevents the emission.
        blockingLatch.countDown()
        assertTrue(resultLatch.await(60, TimeUnit.MILLISECONDS) && completedWithoutBlocking.get())
    }
}
