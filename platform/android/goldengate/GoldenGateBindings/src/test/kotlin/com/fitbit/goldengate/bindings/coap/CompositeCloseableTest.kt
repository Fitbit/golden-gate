package com.fitbit.goldengate.bindings.coap

import com.fitbit.goldengate.bindings.BaseTest
import com.fitbit.goldengate.bindings.util.CompositeCloseable
import com.nhaarman.mockitokotlin2.inOrder
import org.junit.Test
import org.mockito.Mockito
import java.io.Closeable

/**
 * Test for closing CompositeCloseable
 */
class CompositeCloseableTest : BaseTest() {

    @Test
    fun testCompositeCloseableOrder() {
        // Given
        val closeable1 = Mockito.mock(Closeable::class.java)
        val closeable2 = Mockito.mock(Closeable::class.java)
        val closeable3 = Mockito.mock(Closeable::class.java)

        val composite = CompositeCloseable()
        composite.add(closeable1)
        composite.add(closeable2)
        composite.add(closeable3)

        val inOrder = inOrder(closeable3, closeable2, closeable1)

        // When
        composite.close()

        // Then
        inOrder.verify(closeable3).close()
        inOrder.verify(closeable2).close()
        inOrder.verify(closeable1).close()
        assert(composite.isEmpty())

    }
}
