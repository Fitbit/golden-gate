## Logging
The logging subsystem (xp/common/gg_logging.h) provides a simple unified set of logging macros that can be either completely compiled-out, or compiled-in with runtime level-based filters. The configuration of the logging subsystem is completely done at runtime, so you can change the logging level and log output options for each individual file (and possibly each function or code fragment if necessary). The system allows for any number of loggers to exist, and configure which loggers receive which log records, and whether log records bubble up the logger tree.

### Using logging macros
To use logging in a C file, start by declaring the logger name to use for that file. Logger names consist of components separated by '.' characters. Loggers are arranged in a tree structure, where each '.' in the name is a branch in the tree. So for example, a logger named "foo.bar.x" is a child of "foo.bar", which is a child of "foo".
At the top of the tree is always the root logger. The name of the root logger is the empty string.

To declare the logger name for a file, use the macro:
`GG_SET_LOCAL_LOGGER("foo.bar.x")`

Then, in the code, a log record can be emitted by using the macros:
`GG_LOG_XXX(...)`
where `XXX` is the logging level (`FATAL`, `SEVERE` ,`WARNING`, `INFO`, `FINE`, `FINER`, `FINEST`)
and the argument is a printf-compatible (format, params...) list.
Example:
`GG_LOG_FINEST("Hello %s %s", "Golden", "Gate");`

### Using custom loggers
If you need to use multiple loggers in a single C file, or pass the logging level dynamically, advanced macros are available.
You can define a logger using
`GG_DEFINE_LOGGER(<logger-symbol>, <logger-name>)`

Example:
`GG_DEFINE_LOGGER(FooLogger, "gg.test.foo")`

Then you can emit a log record to that specific logger by using the `GG_LOG_L_XXX(...)` macros.

Example:
`GG_LOG_INFO_L(FooLogger, "Hello from FooLogger");`

### Configuring the loggers
Configuration of the loggers is done using a simple syntax. The configuration may come from different sources depending on the system on which the code is running. The most common way to configure the loggers is by passing the log config in an environment variable. Optionally, on systems that have a filesystem, a log configuration file can be used.
When using an environment variable, the `GG_LOG_CONFIG` variable may be set to specify the log configuration source. The simplest source is an in-line configuration, which is signaled by using the form: `plist:xxx` where `xxx` is a sequence of one or more log config entries, separated by ';' characters.

Example: `export GG_LOG_CONFIG=plist:.level=ALL`

If more than one log config source is needed, they are specified in a list, separated by '|' characters.

Example:

Example: `export GG_LOG_CONFIG=plist:.level=ALL|file:my_log_config.properties`

#### plist source
A `plist:` log config is a sequence of one or more configuration properties, separated with ';' characters.

#### Log configuration properties
A log configuration property entry is either a logger configuration property or a log handler configuration property.

##### Logger configuration properties
A logger configuration property has the form: `<logger>.level`, `<logger>.handlers` or `<logger>.forward`.

  - `<logger>.level`: a symbolic or numeric log level for the logger. The logger will only handle log events with a level equal or higher to this level. The level can be set to `OFF` to ignore all log events, or to `ALL` to handle all log events. When no level property is specified for a logger, the logger inherits its parent’s log level.
  - `<logger>.handlers`: a comma-separated list of handlers. The name of the builtin handlers are: `NullHandler`, `ConsoleHandler`, `FileHandler`. Some platforms may have more types of handlers available.
  - `<logger>.forward`: a boolean value. When set to false, the logger does not forward the log event records to its parent. When set to true, the logger forwards log event records to its parent. If this property is not specified, the default behavior of loggers is to forward log even records to their parent.

Boolean values are true or false.  False can be written as `false`, `off`, `no`, or `0`. True can be written as `true`, `on`, `yes`, or `1`.

##### Handler configuration properties
When a logger `<logger>` is configured with a list of handlers, each handler `<handler>` can be configured by setting properties with the prefix `<logger>.<handler>`.

NOTE: the root logger has a ConsoleHandler by default, so this does not need to be specified in the log configuration.

###### Console Handler
`<logger>.ConsoleHandler.colors`: a Boolean value that specifies whether to use ansi terminal color escape sequences.
`<logger>.ConsoleHandler.filter`: a filter mask for the log records. Use a OR combination of:
  - 1: do not include the name of the source file in the log output
  - 2: do not include the timestamp in the log output
  - 4: do not include the function name in the log output
  - 8: do not include the log level in the log output

###### File Handler
`<logger>.FileHandler.filename`: Name of the file to write to. If this property is not specified, FileHandler handlers write to a file with the name `<logger>.log`

### Examples

`export GG_LOG_CONFIG="plist:.level=INFO;.ConsoleHandler.filter=3;foo.bar.y.level=FINE;foo.bar.y.handlers=ConsoleHandler;foo.forward=false"`

This sets the root logger level to `INFO`, the root logger's `ConsoleHandler` filter to 3, and `foo.bar.y` logger level to `FINE`, uses the `ConsoleHandler` to handle log records emitted by the `foo.bar.y` logger, and sets the `foo` logger to not forward to its parent (which by extension means that any logger below `foo`, such as the `foo.bar.y` logger, will not bubble up all the way to the root).
