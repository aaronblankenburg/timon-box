plugins {
    alias(libs.plugins.android.application)
    alias(libs.plugins.kotlin.android)
}

// Die Seite lebt in tools/token-sheet.html und wird von dort ins APK kopiert.
// Bewusst kein zweites Exemplar unter app/src/main/assets: nach der ersten
// Aenderung waere eine der beiden Fassungen veraltet, und man merkt es erst
// auf dem Telefon.
val sheetSource = rootProject.layout.projectDirectory.file("../tools/token-sheet.html").asFile
val generatedAssets = layout.buildDirectory.dir("generated/assets")

val copyTokenSheet by tasks.registering(Copy::class) {
    // Bewusst in eine lokale Variable kopiert: greift der doFirst-Block direkt auf
    // sheetSource zu, faengt er das Skript-Objekt mit ein, und der Configuration
    // Cache kann die Task nicht mehr serialisieren.
    val sheet = sheetSource
    from(sheet)
    into(generatedAssets)
    // Faellt der Pfad einmal weg, soll der Build stehen bleiben und nicht
    // stillschweigend ein APK ohne Seite bauen.
    doFirst {
        check(sheet.isFile) { "tools/token-sheet.html nicht gefunden: $sheet" }
    }
}

android {
    namespace = "dev.aaron.tokensheet"
    compileSdk = 35

    defaultConfig {
        applicationId = "dev.aaron.tokensheet"
        // 31 wie die anderen Apps hier. Ab 29 braucht das Schreiben ueber den
        // MediaStore keine Speicherberechtigung mehr, der Zweig entfaellt also.
        minSdk = 31
        targetSdk = 35
        versionCode = 1
        versionName = "1.0"
    }

    sourceSets {
        getByName("main") {
            assets.srcDir(generatedAssets)
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro"
            )
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    kotlinOptions {
        jvmTarget = "17"
    }
}

tasks.named("preBuild") { dependsOn(copyTokenSheet) }

dependencies {
    implementation(libs.androidx.core.ktx)
    implementation(libs.androidx.activity)
    implementation(libs.androidx.webkit)
}
