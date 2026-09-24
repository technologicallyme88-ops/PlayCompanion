package org.playcompanion.app

import android.os.Bundle
import android.content.ContentResolver
import android.net.Uri
import android.provider.OpenableColumns
import androidx.activity.result.contract.ActivityResultContracts
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.core.content.edit
import androidx.compose.foundation.ExperimentalFoundationApi
import androidx.compose.foundation.combinedClickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.Alignment
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.ui.unit.dp
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleEventObserver
import androidx.lifecycle.compose.LocalLifecycleOwner
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import okhttp3.OkHttpClient
import okhttp3.Call
import okhttp3.MediaType.Companion.toMediaType
import okhttp3.MultipartBody
import okhttp3.RequestBody
import okhttp3.Request
import okhttp3.FormBody
import okio.BufferedSink
import org.json.JSONArray
import java.util.concurrent.TimeUnit
import java.net.DatagramPacket
import java.net.DatagramSocket
import java.net.InetAddress
import java.io.File

data class ReaderFile(val name: String, val size: Long, val directory: Boolean)
data class ReaderStatus(
    val ip: String,
    val mode: String,
    val device: String,
    val version: String,
    val storageTotal: Long,
    val storageUsed: Long
)

data class FirmwareRelease(
    val version: String,
    val publishedAt: String,
    val releaseUrl: String,
    val assetName: String
)

private fun firmwareAssetName(device: String, version: String): String? {
    val identity = "$device $version".lowercase()
    return when {
        "x4 pro" in identity || "x4pro" in identity -> "firmware-pro.bin"
        "x3" in identity -> "firmware-x3.bin"
        "x4" in identity -> "firmware-x4.bin"
        else -> null
    }
}

private fun uniqueFileName(name: String, existing: Set<String>): String {
    if (name !in existing) return name
    val dot = name.lastIndexOf('.')
    val stem = if (dot > 0) name.substring(0, dot) else name
    val extension = if (dot > 0) name.substring(dot) else ""
    var copy = 1
    while ("$stem ($copy)$extension" in existing) copy++
    return "$stem ($copy)$extension"
}

private fun formatBytes(bytes: Long): String = when {
    bytes >= 1_000_000_000L -> "%.1f GB".format(bytes / 1_000_000_000.0)
    bytes >= 1_000_000L -> "%.1f MB".format(bytes / 1_000_000.0)
    bytes >= 1_000L -> "%.1f KB".format(bytes / 1_000.0)
    else -> "$bytes B"
}

private fun isSystemItem(name: String): Boolean =
    name.startsWith(".") || name == "System Volume Information" || name == "XTCache"

private fun versionNumbers(version: String): List<Int> =
    Regex("\\d+").findAll(version).take(3).map { it.value.toInt() }.toList()

private fun isNewerFirmware(latest: String, current: String): Boolean {
    val latestNumbers = versionNumbers(latest)
    val currentNumbers = versionNumbers(current)
    if (latestNumbers.size < 3 || currentNumbers.size < 3) return latest != current
    return latestNumbers.zip(currentNumbers).firstOrNull { it.first != it.second }?.let { it.first > it.second } ?: false
}

private fun isSameFirmwareVersion(latest: String, current: String): Boolean {
    val latestNumbers = versionNumbers(latest)
    val currentNumbers = versionNumbers(current)
    return latestNumbers.size == 3 && latestNumbers == currentNumbers
}

class ReaderApi(
    private val client: OkHttpClient = OkHttpClient.Builder()
        .connectTimeout(15, TimeUnit.SECONDS)
        .readTimeout(5, TimeUnit.MINUTES)
        .writeTimeout(5, TimeUnit.MINUTES)
        .build()
) {
    suspend fun discover(): String = withContext(Dispatchers.IO) {
        DatagramSocket().use { socket ->
            socket.broadcast = true
            socket.soTimeout = 1500
            val payload = "hello".toByteArray()
            socket.send(DatagramPacket(payload, payload.size, InetAddress.getByName("255.255.255.255"), 8134))
            val buffer = ByteArray(256)
            val packet = DatagramPacket(buffer, buffer.size)
            socket.receive(packet)
            "http://${packet.address.hostAddress}"
        }
    }

    suspend fun list(baseUrl: String, path: String): List<ReaderFile> = withContext(Dispatchers.IO) {
        val url = baseUrl.trimEnd('/') + "/api/files?path=" + java.net.URLEncoder.encode(path, "UTF-8")
        client.newCall(Request.Builder().url(url).build()).execute().use { response ->
            check(response.isSuccessful) { "Reader returned HTTP ${response.code}" }
            val json = JSONArray(response.body?.string() ?: "[]")
            buildList {
                for (index in 0 until json.length()) {
                    val item = json.getJSONObject(index)
                    add(ReaderFile(item.getString("name"), item.optLong("size"), item.optBoolean("isDirectory")))
                }
            }.filterNot { isSystemItem(it.name) }
                .sortedWith(compareByDescending<ReaderFile> { it.directory }.thenBy { it.name.lowercase() })
        }
    }

    suspend fun status(baseUrl: String): ReaderStatus = withContext(Dispatchers.IO) {
        val response = client.newCall(Request.Builder().url(baseUrl.trimEnd('/') + "/api/status").build()).execute()
        response.use {
            check(it.isSuccessful)
            val json = org.json.JSONObject(it.body?.string() ?: "{}")
            ReaderStatus(
                ip = json.optString("ip"),
                mode = json.optString("mode"),
                device = json.optString("device"),
                version = json.optString("version"),
                storageTotal = json.optLong("storageTotal"),
                storageUsed = json.optLong("storageUsed")
            )
        }
    }

    suspend fun latestFirmware(assetName: String): FirmwareRelease? = withContext(Dispatchers.IO) {
        val url = "https://api.github.com/repos/technologicallyme88-ops/PlayCompanion/releases/latest"
        client.newCall(
            Request.Builder().url(url).header("Accept", "application/vnd.github+json").build()
        ).execute().use { response ->
            if (response.code == 404) return@use null
            check(response.isSuccessful) { "Release check returned HTTP ${response.code}" }
            val json = org.json.JSONObject(response.body?.string() ?: "{}")
            val hasOtaAsset = json.optJSONArray("assets")?.let { assets ->
                (0 until assets.length()).any { index -> assets.optJSONObject(index)?.optString("name") == assetName }
            } ?: false
            if (!hasOtaAsset) return@use null
            FirmwareRelease(
                version = json.optString("tag_name"),
                publishedAt = json.optString("published_at"),
                releaseUrl = json.optString("html_url"),
                assetName = assetName
            )
        }
    }

    suspend fun upload(baseUrl: String, path: String, uri: Uri, resolver: ContentResolver, filename: String, onProgress: (Long, Long) -> Unit, onCall: (Call) -> Unit = {}) = withContext(Dispatchers.IO) {
        val body = object : RequestBody() {
            override fun contentType() = "application/epub+zip".toMediaType()
            override fun contentLength() = resolver.openAssetFileDescriptor(uri, "r")?.use { it.length } ?: -1L
            override fun writeTo(sink: BufferedSink) {
                val total = contentLength()
                var sent = 0L
                resolver.openInputStream(uri)?.use { input ->
                    val buffer = ByteArray(8192)
                    while (true) {
                        val count = input.read(buffer)
                        if (count < 0) break
                        sink.write(buffer, 0, count)
                        sent += count
                        onProgress(sent, total)
                    }
                }
                    ?: error("Unable to read selected file")
            }
        }
        val url = baseUrl.trimEnd('/') + "/upload?path=" + java.net.URLEncoder.encode(path, "UTF-8")
        val request = Request.Builder().url(url).post(
            MultipartBody.Builder().setType(MultipartBody.FORM)
                .addFormDataPart("file", filename, body).build()
        ).build()
        val call = client.newCall(request)
        onCall(call)
        call.execute().use { response ->
            check(response.isSuccessful) { response.body?.string() ?: "Upload failed (HTTP ${response.code})" }
        }
    }

    suspend fun upload(baseUrl: String, path: String, file: File, onProgress: (Long, Long) -> Unit, onCall: (Call) -> Unit = {}) = withContext(Dispatchers.IO) {
        val body = object : RequestBody() {
            override fun contentType() = "application/epub+zip".toMediaType()
            override fun contentLength() = file.length()
            override fun writeTo(sink: BufferedSink) {
                var sent = 0L
                file.inputStream().use { input ->
                    val buffer = ByteArray(8192)
                    while (true) {
                        val count = input.read(buffer)
                        if (count < 0) break
                        sink.write(buffer, 0, count)
                        sent += count
                        onProgress(sent, file.length())
                    }
                }
            }
        }
        val request = Request.Builder().url(baseUrl.trimEnd('/') + "/upload?path=" + java.net.URLEncoder.encode(path, "UTF-8")).post(
            MultipartBody.Builder().setType(MultipartBody.FORM).addFormDataPart("file", file.name, body).build()
        ).build()
        val call = client.newCall(request)
        onCall(call)
        call.execute().use { response ->
            check(response.isSuccessful) { response.body?.string() ?: "Upload failed (HTTP ${response.code})" }
        }
    }

    suspend fun delete(baseUrl: String, path: String) = post(baseUrl, "/delete", mapOf("path" to path))

    suspend fun delete(baseUrl: String, paths: List<String>) =
        post(baseUrl, "/delete", mapOf("paths" to JSONArray(paths).toString()))

    suspend fun rename(baseUrl: String, path: String, name: String) =
        post(baseUrl, "/rename", mapOf("path" to path, "name" to name))

    suspend fun move(baseUrl: String, path: String, destination: String) =
        post(baseUrl, "/move", mapOf("path" to path, "dest" to destination))

    suspend fun createFolder(baseUrl: String, path: String, name: String) =
        post(baseUrl, "/mkdir", mapOf("path" to path, "name" to name))

    private suspend fun post(baseUrl: String, endpoint: String, fields: Map<String, String>) = withContext(Dispatchers.IO) {
        val form = FormBody.Builder().apply { fields.forEach { (key, value) -> add(key, value) } }.build()
        client.newCall(Request.Builder().url(baseUrl.trimEnd('/') + endpoint).post(form).build()).execute().use { response ->
            check(response.isSuccessful) { response.body?.string() ?: "Request failed (HTTP ${response.code})" }
        }
    }
}

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent { PlayCompanionApp() }
    }
}

@Composable
private fun FirmwarePage(
    reader: ReaderStatus?,
    release: FirmwareRelease?,
    isChecking: Boolean,
    message: String,
    onCheck: () -> Unit
) {
    Column(Modifier.fillMaxSize(), verticalArrangement = Arrangement.spacedBy(16.dp)) {
        Text("Firmware updates", style = MaterialTheme.typography.headlineSmall)
        Card(Modifier.fillMaxWidth()) {
            Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                Text("Reader firmware", style = MaterialTheme.typography.titleMedium)
                Text(reader?.version?.takeIf { it.isNotBlank() } ?: "Connect your reader on the Files tab first")
                reader?.device?.takeIf { it.isNotBlank() }?.let { Text(it, style = MaterialTheme.typography.bodySmall) }
            }
        }
        Card(Modifier.fillMaxWidth()) {
            Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                Text("Latest PlayCompanion release", style = MaterialTheme.typography.titleMedium)
                when {
                    isChecking -> LinearProgressIndicator(Modifier.fillMaxWidth())
                    release == null -> Text("Tap Check for updates to look for a compatible OTA release.")
                    else -> {
                        Text(release.version)
                        Text(release.assetName, style = MaterialTheme.typography.bodySmall)
                        if (release.publishedAt.isNotBlank()) Text("Published ${release.publishedAt.take(10)}", style = MaterialTheme.typography.bodySmall)
                        val current = reader?.version.orEmpty()
                        val updateAvailable = current.isNotBlank() && isNewerFirmware(release.version, current)
                        if (isSameFirmwareVersion(release.version, current)) {
                            Card(
                                colors = CardDefaults.cardColors(containerColor = androidx.compose.ui.graphics.Color(0xFF1F4D2A)),
                                modifier = Modifier.fillMaxWidth()
                            ) {
                                Column(Modifier.padding(16.dp)) {
                                    Text("Firmware is up to date", style = MaterialTheme.typography.titleLarge)
                                    Text("No update is needed for your reader.")
                                }
                            }
                        } else {
                            Text(
                                when {
                                    current.isBlank() -> "Connect the reader to compare versions."
                                    updateAvailable -> "An update is available. On the reader, open Settings → System → Check for updates."
                                    else -> "The reader will make the final compatibility check in Settings → System → Check for updates."
                                },
                                style = MaterialTheme.typography.bodyMedium
                            )
                        }
                    }
                }
                Button(onClick = onCheck, enabled = !isChecking, modifier = Modifier.fillMaxWidth()) {
                    Text("Check for updates")
                }
            }
        }
        Text(message, style = MaterialTheme.typography.bodySmall)
        Text(
            "Updates install directly on the reader after its own confirmation. This app does not upload or flash firmware.",
            style = MaterialTheme.typography.bodySmall
        )
    }
}

@Composable
@OptIn(ExperimentalMaterial3Api::class, ExperimentalFoundationApi::class)
private fun PlayCompanionApp() {
    val context = androidx.compose.ui.platform.LocalContext.current
    val preferences = remember { context.getSharedPreferences("play_companion", android.content.Context.MODE_PRIVATE) }
    var address by remember { mutableStateOf(preferences.getString("reader_address", "http://192.168.4.1") ?: "http://192.168.4.1") }
    var path by remember { mutableStateOf("/") }
    var files by remember { mutableStateOf<List<ReaderFile>>(emptyList()) }
    var selected by remember { mutableStateOf<ReaderFile?>(null) }
    val selectedNames = remember { mutableStateListOf<String>() }
    var renameTarget by remember { mutableStateOf<ReaderFile?>(null) }
    var renameText by remember { mutableStateOf("") }
    var deleteTarget by remember { mutableStateOf<ReaderFile?>(null) }
    var showMovePicker by remember { mutableStateOf(false) }
    var moveDestination by remember { mutableStateOf("/") }
    var moveFolders by remember { mutableStateOf<List<ReaderFile>>(emptyList()) }
    var showAddMenu by remember { mutableStateOf(false) }
    var showCreateFolder by remember { mutableStateOf(false) }
    var newFolderName by remember { mutableStateOf("") }
    var uploadProgress by remember { mutableFloatStateOf(0f) }
    var activeUpload by remember { mutableStateOf<Call?>(null) }
    var status by remember { mutableStateOf<ReaderStatus?>(null) }
    var message by remember { mutableStateOf("Enter the reader address, then tap Connect") }
    var activeTab by remember { mutableIntStateOf(0) }
    var firmwareRelease by remember { mutableStateOf<FirmwareRelease?>(null) }
    var firmwareMessage by remember { mutableStateOf("Check the latest release from GitHub.") }
    var checkingFirmware by remember { mutableStateOf(false) }
    val api = remember { ReaderApi() }
    val scope = rememberCoroutineScope()
    val lifecycleOwner = LocalLifecycleOwner.current
    val currentStatus by rememberUpdatedState(status)
    val currentPath by rememberUpdatedState(path)

    fun load(target: String = path) {
        message = "Checking reader connection…"
        status = null
        selected = null
        selectedNames.clear()
        scope.launch(Dispatchers.Main) {
            runCatching {
                val readerStatus = api.status(address)
                val readerFiles = api.list(address, target)
                readerStatus to readerFiles
            }.onSuccess { (readerStatus, readerFiles) ->
                status = readerStatus
                preferences.edit { putString("reader_address", address); putBoolean("auto_connect", true) }
                files = readerFiles
                path = target
                message = "${readerFiles.size} items"
            }
                .onFailure {
                    status = null
                    message = it.message ?: "Reader is unavailable"
                }
        }
    }

    fun checkFirmware() {
        val reader = status
        val assetName = reader?.let { firmwareAssetName(it.device, it.version) }
        if (reader == null || assetName == null) {
            firmwareRelease = null
            firmwareMessage = if (reader == null) {
                "Connect the reader on the Files tab before checking for updates."
            } else {
                "This reader did not report a supported X3, X4, or X4 Pro device type."
            }
            return
        }
        checkingFirmware = true
        firmwareMessage = "Checking GitHub for $assetName…"
        scope.launch {
            runCatching { api.latestFirmware(assetName) }
                .onSuccess {
                    firmwareRelease = it
                    firmwareMessage = if (it == null) "No compatible $assetName asset is published in the latest release." else "Latest compatible release found."
                }
                .onFailure { firmwareMessage = it.message ?: "Could not check for updates" }
            checkingFirmware = false
        }
    }

    LaunchedEffect(Unit) {
        if (preferences.getBoolean("auto_connect", false)) load("/")
    }

    DisposableEffect(lifecycleOwner) {
        val observer = LifecycleEventObserver { _, event ->
            if (event == Lifecycle.Event.ON_RESUME && currentStatus != null && preferences.getBoolean("auto_connect", false)) {
                load(currentPath)
            }
        }
        lifecycleOwner.lifecycle.addObserver(observer)
        onDispose { lifecycleOwner.lifecycle.removeObserver(observer) }
    }

    LaunchedEffect(showMovePicker, moveDestination) {
        if (showMovePicker) {
            runCatching { api.list(address, moveDestination).filter { it.directory } }
                .onSuccess { moveFolders = it }
                .onFailure { message = it.message ?: "Could not load folders" }
        }
    }

    val picker = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri: Uri? ->
        if (uri != null) {
            message = "Uploading…"
            scope.launch(Dispatchers.Main) {
                runCatching {
                    val filename = context.contentResolver.query(uri, arrayOf(OpenableColumns.DISPLAY_NAME), null, null, null)?.use {
                        if (it.moveToFirst()) it.getString(0) else null
                    } ?: "upload.epub"
                    val existing = api.list(address, path).map { it.name }.toSet()
                    if (filename.endsWith(".epub", ignoreCase = true)) {
                        message = "Optimizing EPUB…"
                        val uploadName = context.contentResolver.openInputStream(uri)?.use { source ->
                            EpubOptimizer.metadataFileName(source, filename)
                        } ?: filename
                        val finalName = uniqueFileName(uploadName, existing)
                        val output = File(context.cacheDir, finalName)
                        context.contentResolver.openInputStream(uri)?.use { source ->
                            EpubOptimizer.optimize(source, output) { uploadProgress = it * 0.5f }
                        } ?: error("Unable to read selected EPUB")
                        message = "Uploading optimized EPUB…"
                        api.upload(address, path, output, { sent, total ->
                            if (total > 0) uploadProgress = 0.5f + sent.toFloat() / total * 0.5f
                        }) { activeUpload = it }
                        output.delete()
                    } else {
                        val finalName = uniqueFileName(filename, existing)
                        api.upload(address, path, uri, context.contentResolver, finalName, { sent, total ->
                            if (total > 0) uploadProgress = sent.toFloat() / total
                        }) { activeUpload = it }
                    }
                }
                    .onSuccess { activeUpload = null; uploadProgress = 0f; message = "Upload complete"; load(path) }
                    .onFailure { activeUpload = null; uploadProgress = 0f; message = if (it is java.io.IOException && it.message?.contains("canceled", true) == true) "Upload cancelled" else it.message ?: "Upload failed" }
            }
        }
    }
    val darkColors = darkColorScheme(
        primary = androidx.compose.ui.graphics.Color(0xFFB8C7FF),
        onPrimary = androidx.compose.ui.graphics.Color(0xFF172B60),
        secondary = androidx.compose.ui.graphics.Color(0xFFBBC7E5),
        background = androidx.compose.ui.graphics.Color(0xFF111318),
        surface = androidx.compose.ui.graphics.Color(0xFF111318),
        surfaceVariant = androidx.compose.ui.graphics.Color(0xFF44474F)
    )

    MaterialTheme(colorScheme = darkColors) {
        Scaffold(
            topBar = {
                TopAppBar(
                    title = { Text("PlayCompanion", style = MaterialTheme.typography.titleLarge) },
                    actions = {
                        if (status == null) {
                            TextButton(onClick = {
                                message = "Searching for reader…"
                                scope.launch {
                                    runCatching { api.discover() }
                                        .onSuccess { address = it; message = "Reader found"; load("/") }
                                        .onFailure { message = "Reader not found" }
                                }
                            }) { Text("Find") }
                            Button(onClick = { load("/") }) { Text("Connect") }
                        } else {
                            Button(
                                onClick = {
                                    status = null
                                    preferences.edit { putBoolean("auto_connect", false) }
                                    message = "Enter or find a reader address"
                                },
                                colors = ButtonDefaults.buttonColors(
                                    containerColor = androidx.compose.ui.graphics.Color(0xFF2E7D32),
                                    contentColor = androidx.compose.ui.graphics.Color.White
                                )
                            ) { Text("Connected") }
                        }
                    }
                )
            },
            floatingActionButton = {
                if (activeTab == 0) Box {
                    Surface(
                        modifier = Modifier.size(56.dp).combinedClickable(
                            onClick = { picker.launch(arrayOf("application/epub+zip", "application/octet-stream")) },
                            onLongClick = { showAddMenu = true }
                        ),
                        shape = CircleShape,
                        color = MaterialTheme.colorScheme.primary,
                        contentColor = MaterialTheme.colorScheme.onPrimary,
                        shadowElevation = 6.dp
                    ) {
                        Box(contentAlignment = Alignment.Center) { Text("+", style = MaterialTheme.typography.headlineMedium) }
                    }
                    DropdownMenu(expanded = showAddMenu, onDismissRequest = { showAddMenu = false }) {
                        DropdownMenuItem(
                            text = { Text("Upload file") },
                            onClick = { showAddMenu = false; picker.launch(arrayOf("application/epub+zip", "application/octet-stream")) }
                        )
                        DropdownMenuItem(
                            text = { Text("New folder") },
                            onClick = { showAddMenu = false; newFolderName = ""; showCreateFolder = true }
                        )
                    }
                }
            },
        ) { padding ->
            Column(Modifier.padding(padding).padding(16.dp).fillMaxSize()) {
                TabRow(selectedTabIndex = activeTab) {
                    Tab(selected = activeTab == 0, onClick = { activeTab = 0 }, text = { Text("Files") })
                    Tab(selected = activeTab == 1, onClick = { activeTab = 1 }, text = { Text("Firmware") })
                }
                Spacer(Modifier.height(16.dp))
                if (activeTab == 1) {
                    FirmwarePage(status, firmwareRelease, checkingFirmware, firmwareMessage, ::checkFirmware)
                } else {
                    if (status == null) {
                    OutlinedTextField(
                        address,
                        { address = it; preferences.edit { putString("reader_address", it) } },
                        label = { Text("Reader address") },
                        singleLine = true,
                        modifier = Modifier.fillMaxWidth()
                    )
                }
                Spacer(Modifier.height(8.dp))
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    if (path != "/") OutlinedButton(onClick = { load(path.substringBeforeLast('/', "/").ifEmpty { "/" }) }) { Text("Up") }
                }
                if (selectedNames.isNotEmpty()) {
                    Column(modifier = Modifier.padding(top = 8.dp)) {
                        Row(horizontalArrangement = Arrangement.spacedBy(8.dp), verticalAlignment = Alignment.CenterVertically) {
                            Text("${selectedNames.size} selected")
                            OutlinedButton(onClick = { moveDestination = "/"; showMovePicker = true }) { Text("Move") }
                            OutlinedButton(onClick = { deleteTarget = ReaderFile("${selectedNames.size} selected items", 0, false) }) { Text("Delete") }
                        }
                        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                            TextButton(onClick = {
                                if (selectedNames.size == files.size) selectedNames.clear()
                                else {
                                    selectedNames.clear()
                                    selectedNames.addAll(files.map { it.name })
                                }
                            }) { Text(if (selectedNames.size == files.size) "Clear all" else "Select all") }
                            TextButton(onClick = { selectedNames.clear() }) { Text("Cancel") }
                        }
                    }
                } else if (selected != null) {
                    Row(horizontalArrangement = Arrangement.spacedBy(8.dp), modifier = Modifier.padding(top = 8.dp)) {
                        OutlinedButton(onClick = {
                            renameTarget = selected
                            renameText = selected?.name.orEmpty()
                        }) { Text("Rename") }
                        OutlinedButton(onClick = {
                            deleteTarget = selected
                        }) { Text("Delete") }
                        OutlinedButton(onClick = {
                            selected?.let {
                                selectedNames.clear()
                                selectedNames.add(it.name)
                                selected = null
                                moveDestination = "/"
                                showMovePicker = true
                            }
                        }) { Text("Move") }
                        TextButton(onClick = { selected = null }) { Text("Cancel") }
                    }
                }
                Spacer(Modifier.height(12.dp))
                status?.let { s ->
                    Card(Modifier.fillMaxWidth()) {
                        Column(Modifier.padding(14.dp)) {
                            Text("Reader connected", style = MaterialTheme.typography.titleMedium)
                            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
                                Text("${s.mode} • ${s.ip}", style = MaterialTheme.typography.bodySmall)
                                Text(
                                    if (s.storageTotal > 0) {
                                        val percent = (s.storageUsed.toDouble() / s.storageTotal * 100).toInt().coerceIn(0, 100)
                                        "${formatBytes(s.storageUsed)} / ${formatBytes(s.storageTotal)} • $percent%"
                                    } else "Storage details unavailable",
                                    style = MaterialTheme.typography.bodySmall
                                )
                            }
                            if (s.storageTotal > 0) {
                                val storageFraction = s.storageUsed.toFloat() / s.storageTotal
                                LinearProgressIndicator(progress = { storageFraction.coerceIn(0f, 1f) }, modifier = Modifier.fillMaxWidth().padding(top = 8.dp))
                            }
                        }
                    }
                    Spacer(Modifier.height(12.dp))
                }
                Text(message, style = MaterialTheme.typography.bodySmall, modifier = Modifier.padding(bottom = 8.dp))
                if (uploadProgress > 0f) {
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        LinearProgressIndicator(progress = { uploadProgress }, modifier = Modifier.weight(1f).padding(bottom = 8.dp))
                        if (activeUpload != null) TextButton(onClick = { activeUpload?.cancel(); message = "Cancelling upload…" }) { Text("Cancel") }
                    }
                }
                Text(path, style = MaterialTheme.typography.titleMedium, modifier = Modifier.padding(vertical = 4.dp))
                if (files.isEmpty()) {
                    Text(
                        "This folder is empty.\nTap + to upload a file or create a folder.",
                        style = MaterialTheme.typography.bodyLarge,
                        modifier = Modifier.padding(top = 32.dp)
                    )
                } else LazyColumn {
                    items(files) { file ->
                        val isSelected = selectedNames.contains(file.name) || selected?.name == file.name
                        ListItem(
                            headlineContent = {
                                Text(
                                    if (isSelected) "✓ " + if (file.directory) "📁 ${file.name}" else file.name
                                    else if (file.directory) "📁 ${file.name}" else file.name
                                )
                            },
                            supportingContent = { if (!file.directory) Text(formatBytes(file.size)) },
                            colors = ListItemDefaults.colors(
                                containerColor = if (isSelected) androidx.compose.ui.graphics.Color(0xFF44366C)
                                else androidx.compose.ui.graphics.Color.Transparent
                            ),
                            modifier = Modifier.combinedClickable(
                                onClick = {
                                    if (selectedNames.isNotEmpty()) {
                                        if (!selectedNames.remove(file.name)) selectedNames.add(file.name)
                                    } else if (selected != null) {
                                        if (selected?.name == file.name) {
                                            selected = null
                                        } else {
                                            selectedNames.add(selected!!.name)
                                            selectedNames.add(file.name)
                                            selected = null
                                        }
                                    } else if (file.directory) {
                                        load("${path.trimEnd('/')}/${file.name}")
                                    } else {
                                        selected = file
                                    }
                                },
                                onLongClick = {
                                    selected = null
                                    if (!selectedNames.remove(file.name)) selectedNames.add(file.name)
                                }
                            )
                        )
                        HorizontalDivider()
                    }
                }
                    }
            }
        }
    }

    renameTarget?.let { item ->
        AlertDialog(
            onDismissRequest = { renameTarget = null },
            title = { Text("Rename file") },
            text = { OutlinedTextField(renameText, { renameText = it }, singleLine = true, label = { Text("Name") }) },
            confirmButton = {
                TextButton(onClick = {
                    val newName = renameText.trim()
                    if (newName.isNotEmpty()) scope.launch {
                        runCatching { api.rename(address, "${path.trimEnd('/')}/${item.name}", newName) }
                            .onSuccess { renameTarget = null; selected = null; message = "Renamed successfully"; load(path) }
                            .onFailure { message = it.message ?: "Rename failed" }
                    }
                }) { Text("Rename") }
            },
            dismissButton = { TextButton(onClick = { renameTarget = null }) { Text("Cancel") } }
        )
    }
    deleteTarget?.let { item ->
        AlertDialog(
            onDismissRequest = { deleteTarget = null },
            title = { Text(if (selectedNames.isEmpty()) "Delete file?" else "Delete selected files?") },
            text = { Text("Delete ${item.name} from the reader?") },
            confirmButton = {
                TextButton(onClick = {
                    scope.launch {
                        val selectedPaths = selectedNames.map { "${path.trimEnd('/')}/$it" }
                        runCatching {
                            if (selectedPaths.isEmpty()) api.delete(address, "${path.trimEnd('/')}/${item.name}")
                            else api.delete(address, selectedPaths)
                        }
                            .onSuccess {
                                deleteTarget = null
                                selected = null
                                selectedNames.clear()
                                message = "Deleted ${item.name}"
                                load(path)
                            }
                            .onFailure {
                                deleteTarget = null
                                message = it.message ?: "Delete failed"
                            }
                    }
                }) { Text("Delete") }
            },
            dismissButton = { TextButton(onClick = { deleteTarget = null }) { Text("Cancel") } }
        )
    }
    if (showMovePicker) {
        AlertDialog(
            onDismissRequest = { showMovePicker = false },
            title = { Text("Move selected files") },
            text = {
                Column {
                    Text("Destination: $moveDestination")
                    if (moveDestination != "/") TextButton(onClick = {
                        moveDestination = moveDestination.substringBeforeLast('/', "/").ifEmpty { "/" }
                    }) { Text("↑ Up") }
                    moveFolders.forEach { folder ->
                        TextButton(onClick = {
                            moveDestination = "${moveDestination.trimEnd('/')}/${folder.name}"
                        }) { Text("📁 ${folder.name}") }
                    }
                }
            },
            confirmButton = {
                TextButton(onClick = {
                        val paths = selectedNames.map { "${path.trimEnd('/')}/$it" }
                        scope.launch {
                            runCatching { paths.forEach { api.move(address, it, moveDestination) } }
                                .onSuccess { showMovePicker = false; selectedNames.clear(); message = "Moved files"; load(path) }
                                .onFailure { showMovePicker = false; message = it.message ?: "Move failed" }
                        }
                    }) { Text("Move here") }
            },
            dismissButton = { TextButton(onClick = { showMovePicker = false }) { Text("Cancel") } }
        )
    }
    if (showCreateFolder) {
        AlertDialog(
            onDismissRequest = { showCreateFolder = false },
            title = { Text("New folder") },
            text = { OutlinedTextField(newFolderName, { newFolderName = it }, label = { Text("Folder name") }, singleLine = true) },
            confirmButton = {
                TextButton(onClick = {
                    val name = newFolderName.trim()
                    if (name.isNotEmpty()) scope.launch {
                        runCatching { api.createFolder(address, path, name) }
                            .onSuccess { showCreateFolder = false; message = "Created $name"; load(path) }
                            .onFailure { showCreateFolder = false; message = it.message ?: "Could not create folder" }
                    }
                }) { Text("Create") }
            },
            dismissButton = { TextButton(onClick = { showCreateFolder = false }) { Text("Cancel") } }
        )
    }
}
