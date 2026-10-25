package dev.aaron.tokensheet

import android.content.Context
import android.net.Uri
import android.provider.OpenableColumns
import android.util.Log
import android.webkit.WebResourceResponse
import androidx.webkit.WebViewAssetLoader
import java.io.File
import java.io.FileInputStream
import java.util.concurrent.ConcurrentHashMap
import java.util.concurrent.atomic.AtomicLong

/** Die virtuelle Herkunft, unter der die App Seite und geteilte Bilder ausliefert. */
const val ASSET_HOST = "appassets.androidplatform.net"

/**
 * Bilder, die von aussen ueber "Teilen" hereinkommen.
 *
 * Sie werden in den Cache kopiert und der Seite unter derselben Herkunft
 * ausgeliefert, unter der auch die Seite selbst laeuft. Der Umweg ueber eine
 * Adresse statt ueber die Bruecke ist Absicht: ein Handyfoto als data:-URL waeren
 * schnell acht Megabyte Base64 quer durch evaluateJavascript. So geht nur eine
 * kurze Adresse hinueber, die Bytes bleiben nativ.
 *
 * Gleiche Herkunft heisst ausserdem: kein CORS. Die Seite laedt das Bild mit
 * crossOrigin="anonymous", und genau deshalb wuerde ein content:// oder file://
 * hier nicht funktionieren.
 */
class SharedImages(context: Context) : WebViewAssetLoader.PathHandler {

    private val dir = File(context.cacheDir, DIR)
    private val files = ConcurrentHashMap<String, Entry>()
    private val counter = AtomicLong()

    private data class Entry(val file: File, val mime: String)

    init {
        dir.mkdirs()
    }

    /** Beim Kaltstart aufraeumen: was noch herumliegt, hat niemand mehr angefordert. */
    fun clear() {
        files.clear()
        runCatching { dir.listFiles()?.forEach { it.delete() } }
    }

    /**
     * Kopiert die geteilte URI in den Cache.
     *
     * Laeuft auf einem Hintergrund-Thread, nicht auf dem UI-Thread: das kann ein
     * 12-Megapixel-Foto sein.
     *
     * @return die Adresse, unter der die Seite das Bild laden kann, oder null.
     */
    fun add(context: Context, uri: Uri): String? {
        val id = counter.incrementAndGet().toString()
        val file = File(dir, id)
        return try {
            context.contentResolver.openInputStream(uri).use { input ->
                requireNotNull(input) { "kein Stream fuer $uri" }
                file.outputStream().use { input.copyTo(it) }
            }
            val size = file.length()
            // Leere Datei waere ein stiller Fehlschlag, eine riesige ein OOM beim Dekodieren.
            check(size in 1..MAX_BYTES) { "unbrauchbare Groesse: $size Byte" }

            val mime = context.contentResolver.getType(uri)?.takeIf { it.startsWith("image/") }
                ?: "image/jpeg"
            files[id] = Entry(file, mime)

            // Der Dateiname steht nur im Pfad, damit die Seite ihn als Beschriftung
            // anzeigen kann; nachgeschlagen wird ueber die id davor.
            "https://$ASSET_HOST/$PREFIX$id/${Uri.encode(displayName(context, uri))}"
        } catch (e: Exception) {
            Log.e(TAG, "geteiltes Bild uebernehmen", e)
            file.delete()
            null
        }
    }

    /** Der Pfad kommt ohne das Praefix an, also "<id>/<name>". */
    override fun handle(path: String): WebResourceResponse? {
        val entry = files[path.substringBefore('/')] ?: return null
        return try {
            WebResourceResponse(entry.mime, null, FileInputStream(entry.file))
        } catch (e: Exception) {
            Log.e(TAG, "ausliefern: $path", e)
            null
        }
    }

    private fun displayName(context: Context, uri: Uri): String {
        val name = runCatching {
            context.contentResolver.query(uri, arrayOf(OpenableColumns.DISPLAY_NAME), null, null, null)
                ?.use { c -> if (c.moveToFirst()) c.getString(0) else null }
        }.getOrNull() ?: uri.lastPathSegment
        // Schraegstriche wuerden den Pfad zerlegen, den die Seite spaeter wieder zerpflueckt.
        return (name ?: "shared.jpg").substringAfterLast('/').ifBlank { "shared.jpg" }
    }

    companion object {
        const val PREFIX = "shared/"
        private const val TAG = "TokenSheetShared"
        private const val DIR = "incoming"
        private const val MAX_BYTES = 64L * 1024 * 1024
    }
}
