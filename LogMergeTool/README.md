# Log Merge Tool

This tool is designed to read through a folder containing Couchbase Lite logs in either binary or text format and merge them into one or more files in a given output style.

### Motivation

Couchbase Lite logs write a single level of logging per file (in other words, a verbose logs contains *only* verbose log lines, an info log contains *only* info log lines) to get around the following issues:

1. If all logging went to one file, then verbose logs would quickly cause a log rotation and other levels would be lost earlier than needed.  In the above method, even if the verbose log rotates, the other information from further back will still be available.

2. If creating logs like Sync Gateway, which will create logs which write their level and higher (e.g. warn will contain warn *and* error logs) then disk space, which is at a premium on mobile devices, is wasted.

However, this makes putting together a flow of time harder since the flow will span multiple files.  This tool will merge those files back together so that the flow can be more easily followed.

### Usage

```
LogMergeTool [options]

Options:
  -?                       Show help information
  -f|--force               Automatically overwrite existing log files
  -p|--log-path <PATH>     The path to the folder containing log files to merge
  -o|--output <PATH>       The file to write the results to (or base file name for multi file results)
  -t|--output-type <TYPE>  The format to write the output as (default SingleFile)
  ```

  ### Output Types

  | Type | Description |
  | - | - |
  |SingleFile | Merges all logs into a single readable text file (or multiple if advanced configuration present) |
  |SyncGateway | Creates Sync Gateway style inclusive logs for each level |
  |LogLady | Creates a merged binary log file which is readable by [LogLady](htts://github.com/couchbaselabs/LogLady)

  ### Advanced Configuration

  Advanced configuration can be provided by placing a `config.ini` file into the same directory as the executable.  The following options are supported (given in group:key format):

  | Option | Description | Format | Default |
  |-|-|-|-|
  | single_file:max_size | If set, when a single file output reaches the given size the file will rotate | /d+([GgMmKk]?]) | Empty |
  | format:use_utc | For formats that log a readable timestamp, this will cause the output to contain UTC time instead of the local system time | true\|false | false |

  **Example**
  ```
  [single_file]
  max_size=10M

  [format]
  use_utc=true
  ```