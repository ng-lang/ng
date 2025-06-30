package net.kimleo.ng.util


infix fun <T, R> T?.bind(fn: (T) -> R): R? {
    if (this != null) {
        return fn(this)
    }
    return null
}

infix fun <T> T?.orElse(t: () -> T): T {
    if (this != null)
        return this
    return t()
}
