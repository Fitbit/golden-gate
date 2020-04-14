-keep public class * {
    public protected *;
}

# essentially remove verbose, debug, and info logs
-assumenosideeffects class timber.log.Timber {
    public static *** v(...);
    public static *** d(...);
    public static *** i(...);
}
