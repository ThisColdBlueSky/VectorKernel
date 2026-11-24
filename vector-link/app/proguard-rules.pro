# R8 / ProGuard Optimization Rules for Vector Link

# Aggressive optimizations
-allowaccessmodification
-repackageclasses

# TopJohnWu's libsu reflection & IPC
-keep class com.topjohnwu.superuser.** { *; }
-dontwarn com.topjohnwu.superuser.**

# Keep app entry points & viewmodels
-keep class io.github.rhythmcache.vector.link.MainActivity { *; }
-keep class io.github.rhythmcache.vector.link.VectorViewModel { *; }
-keep class io.github.rhythmcache.vector.link.Snapshot { *; }

# Kotlin coroutines
-dontwarn kotlinx.coroutines.**
