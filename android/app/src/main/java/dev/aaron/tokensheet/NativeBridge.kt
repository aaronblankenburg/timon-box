package dev.aaron.tokensheet

import android.app.Activity
import android.content.ContentValues
import android.content.Intent
import android.os.Environment
import android.provider.MediaStore
import android.util.Base64
import android.util.Log
import android.webkit.JavascriptInterface
import android.widget.Toast
import androidx.core.content.FileProvider
import java.io.File

/**
 * Die Bruecke zwischen der Seite und Android.
 *
 * Sie existiert, weil die WebView zwei Dinge nicht kann, die die Seite im Browser
 * benutzt: `navigator.share` gibt es in der WebView gar nicht (das ist keine Frage
 * von http gegen https, die Web-Share-API ist dort schlicht nicht implementiert),
 * und ein `<a download>` tut nichts. Beides wird hier nativ nachgereicht — und
 * dabei gleich besser: der echte Teilen-Dialog und ein Eintrag in der Galerie
 * statt eines Browser-Downloads.
 *
 * Alle @JavascriptInterface-Methoden laufen auf einem Binder-Thread, nicht auf dem
 * UI-Thread. Alles Sichtbare muss deshalb durch runOnUiThread.
 */
class NativeBridge(
    private val activity: Activity,
    private val onReady: () -> Unit,
) {

    /**
     * Die Seite meldet, dass sie steht. Erst danach darf die App geteilte Bilder
     * nachreichen: vorher wuerde das Wiederherstellen der Sitzung sie ueberschreiben.
     */
    @JavascriptInterface
    fun onPageReady() {
        activity.runOnUiThread { onReady() }
    }

    /** Teilt das fertige JPEG ueber den Android-Teilen-Dialog. */
    @JavascriptInterface
    fun shareJpeg(base64: String, filename: String) {
        try {
            val dir = File(activity.cacheDir, SHARE_DIR).apply { mkdirs() }
            // Alte Staende wegraeumen: die Datei heisst bei jedem Bogen fast gleich,
            // und der Cache soll nicht mit Bogen von vorletzter Woche volllaufen.
            dir.listFiles()?.forEach { it.delete() }

            val file = File(dir, safeName(filename))
            file.writeBytes(Base64.decode(base64, Base64.DEFAULT))

            val uri = FileProvider.getUriForFile(activity, "${activity.packageName}.fileprovider", file)
            val send = Intent(Intent.ACTION_SEND).apply {
                type = MIME
                putExtra(Intent.EXTRA_STREAM, uri)
                // Ohne dieses Flag darf die Ziel-App die URI nicht lesen und
                // bekommt eine leere Datei.
                addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
            }
            activity.runOnUiThread {
                activity.startActivity(Intent.createChooser(send, null))
            }
        } catch (e: Exception) {
            Log.e(TAG, "shareJpeg", e)
            toast("Teilen fehlgeschlagen")
        }
    }

    /**
     * Legt das JPEG unter Bilder/Token Sheet ab. Ersetzt den Download-Link der
     * Seite, der in der WebView ins Leere liefe. Ab minSdk 31 braucht das Schreiben
     * ueber den MediaStore keine Speicherberechtigung.
     *
     * @return true, wenn die Datei in der Galerie steht.
     */
    @JavascriptInterface
    fun saveToGallery(base64: String, filename: String): Boolean {
        val resolver = activity.contentResolver
        var uri: android.net.Uri? = null
        return try {
            val values = ContentValues().apply {
                put(MediaStore.Images.Media.DISPLAY_NAME, safeName(filename))
                put(MediaStore.Images.Media.MIME_TYPE, MIME)
                put(MediaStore.Images.Media.RELATIVE_PATH, "${Environment.DIRECTORY_PICTURES}/$ALBUM")
                // Solange IS_PENDING steht, sieht die Galerie die halbe Datei nicht.
                put(MediaStore.Images.Media.IS_PENDING, 1)
            }
            uri = resolver.insert(MediaStore.Images.Media.EXTERNAL_CONTENT_URI, values)
                ?: return false.also { toast("Speichern fehlgeschlagen") }

            resolver.openOutputStream(uri).use { out ->
                requireNotNull(out).write(Base64.decode(base64, Base64.DEFAULT))
            }
            resolver.update(uri, ContentValues().apply {
                put(MediaStore.Images.Media.IS_PENDING, 0)
            }, null, null)
            true
        } catch (e: Exception) {
            Log.e(TAG, "saveToGallery", e)
            // Halbe Datei nicht in der Galerie stehen lassen.
            uri?.let { runCatching { resolver.delete(it, null, null) } }
            toast("Speichern fehlgeschlagen")
            false
        }
    }

    private fun toast(text: String) = activity.runOnUiThread {
        Toast.makeText(activity, text, Toast.LENGTH_SHORT).show()
    }

    /** Der Name kommt aus der Seite — alles, was einen Pfad ergeben koennte, raus. */
    private fun safeName(name: String): String {
        val cleaned = name.substringAfterLast('/').substringAfterLast('\\')
            .filter { it.isLetterOrDigit() || it in "-_. " }
            .trim()
        return if (cleaned.endsWith(".jpg", true) && cleaned.length > 4) cleaned else "token-sheet.jpg"
    }

    private companion object {
        const val TAG = "TokenSheetBridge"
        const val MIME = "image/jpeg"
        const val ALBUM = "Token Sheet"
        const val SHARE_DIR = "share"
    }
}
