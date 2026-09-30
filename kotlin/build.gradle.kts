plugins {
    kotlin("jvm") version "1.9.24"
    `java-library`
}

group = "io.aegis"
version = "1.0.0"

repositories { mavenCentral() }

dependencies {
    testImplementation(kotlin("test"))
}

kotlin {
    jvmToolchain(17)
}