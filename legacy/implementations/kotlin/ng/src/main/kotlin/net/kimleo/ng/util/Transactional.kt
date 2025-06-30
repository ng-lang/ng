package net.kimleo.ng.util

interface Transactional<T> {
    var state: T
    var savedState: T?
    fun begin() {
        synchronized(this) {
            if (savedState == null) {
                savedState = state
            } else throw TransactionError("Already in a transaction")
        }
    }

    fun commit() {
        synchronized(this) {
            if (savedState != null) {
                savedState = null
            } else throw TransactionError("No transaction found")
        }
    }

    fun rollback() {
        synchronized(this) {
            if (savedState != null) {
                state = savedState!!
                savedState = null
            } else throw TransactionError("No transaction found")
        }
    }

    fun transactional(fn : () -> Unit) {
        begin()
        try {
            fn()
            commit()
        } catch (ex: Throwable) {
            rollback()
        }
    }
}


