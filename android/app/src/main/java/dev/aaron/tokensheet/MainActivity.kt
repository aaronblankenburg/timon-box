package dev.aaron.tokensheet

import android.content.Intent
import android.content.pm.ApplicationInfo
import android.net.Uri
import android.os.Bundle
import android.provider.MediaStore
import android.util.Log
import android.view.ViewGroup
import android.webkit.ValueCallback
import android.webkit.WebChromeClient
import android.webkit.WebResourceRequest
import android.webkit.WebResourceResponse
import android.webkit.WebView
import android.webkit.WebViewClient
import androidx.activity.ComponentActivity
import androidx.activity.OnBackPressedCallback
import androidx.activity.result.ActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.core.content.FileProvider
import androidx.core.content.IntentCompat
import androidx.webkit.WebViewAssetLoader
import org.json.JSONObject
import java.io.File
import java.util.concurrent.Executors

/**
 * Die ganze App: ein bildschirmfuellender WebView, der tools/token-sheet.html aus
 * den App-Assets laedt, plus das, was die WebView von sich aus nicht mitbringt.
 */
class MainActivity : ComponentActivity() {

    private lateinit var web: WebView

    /** Wartet auf das Ergebnis des Dateiauswahl-Dialogs der Seite. */
    private var pendingFiles: ValueCallback<Array<Uri>>? = null

    /** Ziel der Kamera-Aufnahme. Die Kamera-App schreibt dorthin, nicht in das Intent-Ergebnis. */
    private var captureTarget: Uri? = null

    /** Bilder, die andere Apps ueber "Teilen" hereinreichen. */
    private val shared by lazy { SharedImages(this) }

    /** Ein Foto kopieren kann dauern; das gehoert nicht auf den UI-Thread. */
    private val io = Executors.newSingleThreadExecutor()

    /**
     * Geteilte Bilder, die noch warten. Sie duerfen erst in die Seite, wenn die
     * ihre gespeicherte Sitzung geladen hat — sonst ueberschreibt restore() sie
     * gleich wieder, denn es beschreibt alle sechs Plaetze.
     */
    private val pendingShares = mutableListOf<String>()
    private var pageReady = false

    private val filePicker = registerForActivityResult(
        ActivityResultContracts.StartActivityForResult(), ::onFilesPicked
    )

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        val debuggable = applicationInfo.flags and ApplicationInfo.FLAG_DEBUGGABLE != 0
        WebView.setWebContentsDebuggingEnabled(debuggable)

        // Die Seite kommt ueber den AssetLoader unter einer https-Herkunft, nicht als
        // file://. Das ist kein Geschmack: auf file:// hat die WebView kein IndexedDB,
        // und genau dort liegt die angefangene Sitzung der Seite. Nebenbei ist es ein
        // echter sicherer Kontext, und die Herkunft bleibt ueber App-Updates gleich,
        // die Sitzung ueberlebt also eine neue Fassung.
        shared.clear()
        val assets = WebViewAssetLoader.Builder()
            .addPathHandler("/assets/", WebViewAssetLoader.AssetsPathHandler(this))
            .addPathHandler("/${SharedImages.PREFIX}", shared)
            .build()

        web = WebView(this).apply {
            layoutParams = ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT
            )
            settings.javaScriptEnabled = true
            settings.domStorageEnabled = true   // localStorage: die Hell/Dunkel-Wahl der Seite
            // Kein Zoom-Gehabe: die Seite bringt ihr eigenes Pinch-Zoom im Zuschneiden mit.
            settings.builtInZoomControls = false
            settings.setSupportZoom(false)

            webViewClient = object : WebViewClient() {
                override fun shouldInterceptRequest(
                    view: WebView, request: WebResourceRequest
                ): WebResourceResponse? = assets.shouldInterceptRequest(request.url)

                override fun shouldOverrideUrlLoading(
                    view: WebView, request: WebResourceRequest
                ): Boolean {
                    if (request.url.host == ASSET_HOST) return false
                    // Die Seite selbst verlinkt nichts nach draussen; falls doch
                    // einmal etwas dazukommt, gehoert es in den Browser, nicht hierher.
                    runCatching { startActivity(Intent(Intent.ACTION_VIEW, request.url)) }
                    return true
                }
            }

            webChromeClient = object : WebChromeClient() {
                override fun onShowFileChooser(
                    view: WebView,
                    callback: ValueCallback<Array<Uri>>,
                    params: FileChooserParams
                ): Boolean = openFilePicker(callback, params)
            }

            addJavascriptInterface(
                NativeBridge(this@MainActivity, onReady = ::markPageReady), BRIDGE_NAME
            )
        }

        setContentView(web)
        web.loadUrl("https://$ASSET_HOST/assets/token-sheet.html")

        handleShare(intent)

        // Die Seite ist ein einziges Dokument, canGoBack() ist also immer false.
        // Zurueck soll trotzdem einen Schritt zurueck gehen, nicht die App schliessen.
        onBackPressedDispatcher.addCallback(this, object : OnBackPressedCallback(true) {
            override fun handleOnBackPressed() {
                web.evaluateJavascript("(window.TokenSheet&&TokenSheet.state.step)|0") { value ->
                    val step = value?.trim()?.toIntOrNull() ?: 0
                    if (step > 1) {
                        web.evaluateJavascript("TokenSheet.go(${step - 1})", null)
                    } else {
                        isEnabled = false
                        onBackPressedDispatcher.onBackPressed()
                    }
                }
            }
        })
    }

    // -----------------------------------------------------------------------
    // Teilen aus anderen Apps
    // -----------------------------------------------------------------------

    // Die Activity laeuft als singleTask, ein Teilen bei laufender App landet
    // deshalb hier statt in einer zweiten Instanz.
    override fun onNewIntent(intent: Intent) {
        super.onNewIntent(intent)
        setIntent(intent)
        handleShare(intent)
    }

    private fun handleShare(intent: Intent?) {
        // Der Intent kommt aus einer fremden App und ist damit nichts, worauf man
        // sich verlassen sollte. Ein kaputtes Extra darf hier nicht die App killen.
        val uris: List<Uri> = try {
            when (intent?.action) {
                Intent.ACTION_SEND ->
                    listOfNotNull(IntentCompat.getParcelableExtra(intent, Intent.EXTRA_STREAM, Uri::class.java))
                Intent.ACTION_SEND_MULTIPLE ->
                    IntentCompat.getParcelableArrayListExtra(intent, Intent.EXTRA_STREAM, Uri::class.java).orEmpty()
                else -> emptyList()
            }
        } catch (e: Exception) {
            Log.e(TAG, "geteilten Intent lesen", e)
            emptyList()
        }
        if (uris.isEmpty()) return

        // Die Leseerlaubnis auf die URIs haengt an diesem Intent, deshalb sofort kopieren.
        io.execute {
            val urls = uris.mapNotNull { shared.add(this, it) }
            runOnUiThread {
                pendingShares += urls
                if (pageReady) flushShares()
            }
        }
    }

    /** Von der Seite gemeldet, sobald sie steht. */
    private fun markPageReady() {
        pageReady = true
        flushShares()
    }

    private fun flushShares() {
        if (pendingShares.isEmpty()) return
        // addUrls trennt an Leerzeichen; die Adressen enthalten keine.
        val arg = JSONObject.quote(pendingShares.joinToString(" "))
        pendingShares.clear()
        web.evaluateJavascript("window.TokenSheet&&TokenSheet.receiveShared($arg)", null)
    }

    /**
     * Ohne diesen Weg passiert bei "Choose pictures" und "Take photo" gar nichts:
     * die WebView oeffnet von sich aus keinen Dateiauswahl-Dialog.
     */
    private fun openFilePicker(
        callback: ValueCallback<Array<Uri>>,
        params: WebChromeClient.FileChooserParams
    ): Boolean {
        pendingFiles?.onReceiveValue(null)   // ein haengender Dialog blockiert sonst dauerhaft
        pendingFiles = callback
        captureTarget = null

        val pick = Intent(Intent.ACTION_GET_CONTENT).apply {
            type = "image/*"
            addCategory(Intent.CATEGORY_OPENABLE)
            putExtra(
                Intent.EXTRA_ALLOW_MULTIPLE,
                params.mode == WebChromeClient.FileChooserParams.MODE_OPEN_MULTIPLE
            )
        }

        // Die Kamera laeuft ueber den Intent der Kamera-App. Deshalb steht CAMERA
        // bewusst NICHT im Manifest: waere die Berechtigung deklariert, wuerde
        // Android sie auch fuer diesen Intent verlangen.
        val camera = cameraIntent()

        // "Take photo" traegt in der Seite das capture-Attribut. Dann direkt die
        // Kamera oeffnen statt erst einen Auswahldialog zu zeigen.
        val intent = if (params.isCaptureEnabled && camera != null) {
            camera
        } else {
            Intent.createChooser(pick, null).apply {
                camera?.let { putExtra(Intent.EXTRA_INITIAL_INTENTS, arrayOf(it)) }
            }
        }

        return try {
            filePicker.launch(intent)
            true
        } catch (e: Exception) {
            Log.e(TAG, "Dateiauswahl", e)
            pendingFiles = null
            callback.onReceiveValue(null)
            false
        }
    }

    private fun cameraIntent(): Intent? = try {
        val dir = File(cacheDir, "capture").apply { mkdirs() }
        dir.listFiles()?.forEach { it.delete() }
        val file = File(dir, "capture-${System.currentTimeMillis()}.jpg")
        val uri = FileProvider.getUriForFile(this, "$packageName.fileprovider", file)
        captureTarget = uri
        Intent(MediaStore.ACTION_IMAGE_CAPTURE).apply {
            // Ohne EXTRA_OUTPUT liefert die Kamera nur ein Vorschaubildchen zurueck.
            putExtra(MediaStore.EXTRA_OUTPUT, uri)
            addFlags(Intent.FLAG_GRANT_WRITE_URI_PERMISSION)
        }.takeIf { it.resolveActivity(packageManager) != null }
    } catch (e: Exception) {
        Log.e(TAG, "Kamera-Intent", e)
        captureTarget = null
        null
    }

    private fun onFilesPicked(result: ActivityResult) {
        val callback = pendingFiles ?: return
        pendingFiles = null
        val camera = captureTarget
        captureTarget = null

        // Das Ergebnis kommt aus einer fremden App. Was hier hochfliegt, reisst
        // ohne dieses Netz die ganze App mit — genau so ist die Mehrfachauswahl
        // einmal abgestuerzt. Und die Seite muss in jedem Fall eine Antwort
        // bekommen, sonst laesst sich der Auswahl-Dialog nie wieder oeffnen.
        val uris = try {
            if (result.resultCode != RESULT_OK) {
                null
            } else {
                // Galerie-Auswahl, eine oder mehrere ...
                urisFrom(result.data)
                    // ... die Kamera dagegen schreibt in die URI aus EXTRA_OUTPUT
                    // und liefert gar kein Intent. Nur dann dort nachsehen.
                    ?: camera?.takeIf(::hasContent)?.let { arrayOf(it) }
            }
        } catch (e: Exception) {
            Log.e(TAG, "Auswahl auswerten", e)
            null
        }

        callback.onReceiveValue(uris)
    }

    /**
     * Absichtlich nicht FileChooserParams.parseResult: das sieht ausschliesslich
     * auf data.data. Bei Mehrfachauswahl liegen die Bilder aber in data.clipData,
     * parseResult liefert dann null — und die App landete frueher im Kamera-Zweig
     * und stuerzte an einer Datei ab, die nie geschrieben wurde.
     */
    private fun urisFrom(data: Intent?): Array<Uri>? {
        if (data == null) return null
        data.clipData?.let { clip ->
            val list = (0 until clip.itemCount).mapNotNull { clip.getItemAt(it)?.uri }
            if (list.isNotEmpty()) return list.toTypedArray()
        }
        return data.data?.let { arrayOf(it) }
    }

    /** Prueft, ob hinter der URI wirklich etwas steht. Wirft nicht. */
    private fun hasContent(uri: Uri): Boolean = runCatching {
        contentResolver.openInputStream(uri)?.use { it.read() != -1 } == true
    }.getOrElse {
        Log.w(TAG, "leere oder unlesbare URI: $uri", it)
        false
    }

    override fun onDestroy() {
        pendingFiles?.onReceiveValue(null)
        pendingFiles = null
        io.shutdown()
        super.onDestroy()
    }

    private companion object {
        const val TAG = "TokenSheet"
        const val BRIDGE_NAME = "TokenSheetNative"
    }
}
