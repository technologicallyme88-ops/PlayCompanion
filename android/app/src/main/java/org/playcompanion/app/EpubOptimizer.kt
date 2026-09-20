package org.playcompanion.app

import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.os.Build
import java.io.ByteArrayOutputStream
import java.io.File
import java.io.InputStream
import java.nio.charset.StandardCharsets
import java.util.zip.CRC32
import java.util.zip.ZipEntry
import java.util.zip.ZipInputStream
import java.util.zip.ZipOutputStream

/** Rebuilds EPUB archives locally so large embedded images are reader-friendly before upload. */
object EpubOptimizer {
    private const val maxWidth = 480
    private const val maxHeight = 800

    fun optimize(input: InputStream, output: File, onProgress: (Float) -> Unit): File {
        ZipInputStream(input.buffered()).use { zip ->
            ZipOutputStream(output.outputStream().buffered()).use { out ->
                var entry = zip.nextEntry
                var index = 0
                while (entry != null) {
                    val bytes = zip.readBytes()
                    val extension = entry.name.substringAfterLast('.', "").lowercase()
                    val isImage = extension in setOf("jpg", "jpeg", "png", "webp")
                    val data = if (isImage) optimizeImage(bytes, extension) ?: bytes else bytes
                    val target = ZipEntry(entry.name)
                    if (entry.name == "mimetype") {
                        target.method = ZipEntry.STORED
                        target.size = data.size.toLong()
                        target.compressedSize = data.size.toLong()
                        target.crc = CRC32().apply { update(data) }.value
                    }
                    out.putNextEntry(target)
                    out.write(data)
                    out.closeEntry()
                    zip.closeEntry()
                    index++
                    onProgress((index % 100) / 100f)
                    entry = zip.nextEntry
                }
            }
        }
        onProgress(1f)
        return output
    }

    fun metadataFileName(input: InputStream, fallback: String): String {
        ZipInputStream(input.buffered()).use { zip ->
            var entry = zip.nextEntry
            while (entry != null) {
                if (entry.name.lowercase().endsWith(".opf")) {
                    val opf = String(zip.readBytes(), StandardCharsets.UTF_8)
                    val title = xmlValue(opf, "title")
                    val author = xmlValue(opf, "creator")
                    if (!title.isNullOrBlank() && !author.isNullOrBlank()) {
                        return "${safeFilePart(title)} - ${safeFilePart(author)}.epub"
                    }
                    break
                }
                zip.closeEntry()
                entry = zip.nextEntry
            }
        }
        return fallback.removeSuffix(".epub") + ".epub"
    }

    private fun xmlValue(xml: String, element: String): String? {
        val match = Regex("<dc:$element\\b[^>]*>(.*?)</dc:$element>", setOf(RegexOption.IGNORE_CASE, RegexOption.DOT_MATCHES_ALL))
            .find(xml)?.groupValues?.getOrNull(1)?.replace(Regex("<[^>]+>"), "")?.trim()
        return match?.replace("&amp;", "&")?.replace("&quot;", "\"")?.replace("&apos;", "'")
    }

    private fun safeFilePart(value: String): String =
        value.replace(Regex("[\\\\/:*?\"<>|]"), " ").replace(Regex("\\s+"), " ").trim().take(120)

    private fun optimizeImage(source: ByteArray, extension: String): ByteArray? {
        if (extension == "webp" && Build.VERSION.SDK_INT < Build.VERSION_CODES.R) return null
        val bitmap = BitmapFactory.decodeByteArray(source, 0, source.size) ?: return null
        try {
            val image = bitmap
            val scale = minOf(1f, maxWidth.toFloat() / image.width, maxHeight.toFloat() / image.height)
            val width = (image.width * scale).toInt().coerceAtLeast(1)
            val height = (image.height * scale).toInt().coerceAtLeast(1)
            val resized = if (width != image.width || height != image.height) Bitmap.createScaledBitmap(image, width, height, true) else image
            return ByteArrayOutputStream().use { bytes ->
                val format = when (extension) {
                    "png" -> Bitmap.CompressFormat.PNG
                    "webp" -> Bitmap.CompressFormat.WEBP_LOSSY
                    else -> Bitmap.CompressFormat.JPEG
                }
                val quality = if (extension == "png") 100 else 85
                resized.compress(format, quality, bytes)
                if (resized !== image) resized.recycle()
                bytes.toByteArray()
            }
        } finally {
            bitmap.recycle()
        }
    }
}
