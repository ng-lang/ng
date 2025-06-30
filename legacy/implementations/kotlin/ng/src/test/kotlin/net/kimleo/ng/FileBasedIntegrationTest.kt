package net.kimleo.ng

import java.io.BufferedReader
import java.io.File
import java.io.FileInputStream
import java.io.InputStreamReader

abstract class FileBasedIntegrationTest {
    protected fun open(name: String): BufferedReader {
        return BufferedReader(
                InputStreamReader(
                        FileInputStream(
                                File(javaClass.classLoader.getResource(name).file))))
    }
}
