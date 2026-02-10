// ============================================================================
// Built-in Function Handling Module
// ============================================================================
// This file contains the implementation of built-in function handling for
// the LLVM code generator. It should be included by llvm_codegen.cpp, not
// compiled separately.
//
// To use this module, add the following line in llvm_codegen.cpp after
// the other includes:
//   #include "llvm_codegen_builtin.cpp"
// ============================================================================

// ============================================================================
// Built-in Function Handling
// ============================================================================

bool LLVMCodeGen::isBuiltinFunction(const std::string& name) {
    // Apply alias mapping if available
    std::string mappedName = name;
    if (aliasMap && aliasMap->isLoaded()) {
        mappedName = aliasMap->mapBuiltin(name);
    }
    
    static const std::set<std::string> builtins = {
        // I/O and type
        "print", "input", "type", "len", "str", "int", "float", "number", "bool",
        // Math basics
        "abs", "min", "max", "power", "pow", "sqrt", "random_int", "random_float", "random",
        // Math trig
        "sin", "cos", "tan", "asin", "acos", "atan", "atan2",
        "sinh", "cosh", "tanh", "hypot",
        // Math other
        "floor", "ceil", "round", "log", "log10", "log2", "exp",
        "degrees", "radians", "clamp", "lerp", "sign", "mean", "median",
        // System
        "time", "sleep", "shell", "shell_output", "env", "set_env", "exit", "argv",
        "platform", "getpid", "system", "exec",
        // Memory management (MCU/Embedded)
        "mem_stats", "mem_reset", "target_info",
        // GC (Garbage Collection - Cycle Detection)
        "gc_collect", "gc_enable", "gc_set_threshold", "gc_stats", "gc_debug",
        // HAL - GPIO
        "gpio_init", "gpio_write", "gpio_read", "gpio_deinit",
        // HAL - PWM
        "pwm_init", "pwm_write", "pwm_deinit",
        // HAL - ADC
        "adc_init", "adc_read", "adc_deinit",
        // HAL - I2C
        "i2c_init", "i2c_write", "i2c_read", "i2c_deinit",
        // HAL - SPI
        "spi_init", "spi_transfer", "spi_deinit",
        // HAL - UART
        "uart_init", "uart_write", "uart_read", "uart_available", "uart_deinit",
        // HAL - Delay/Timer
        "delay_ms", "delay_us", "millis", "micros",
        // HAL - System
        "hal_init", "hal_deinit", "hal_platform", "hal_debug",
        // Files
        "read_file", "write_file", "append_file", "exists", "is_file", "is_dir",
        "list_dir", "create_dir", "file_size", "getcwd", "cd",
        "join_path", "basename", "dirname", "extension", "absolute_path",
        "copy_file", "move_file", "remove_file", "remove_dir",
        // String encryption
        "decrypt_string",
        // JSON
        "json_encode", "json_decode", "format",
        // Regular expressions
        "regex_match", "regex_search", "regex_test",
        "regex_groups", "regex_named",
        "regex_find_all", "regex_find_all_groups",
        "regex_replace", "regex_replace_all",
        "regex_split", "regex_split_n",
        "regex_compile", "regex_match_compiled", "regex_search_compiled",
        "regex_find_all_compiled", "regex_replace_compiled", "regex_free",
        "regex_escape", "regex_error",
        // List/Dict operations
        "append", "pop", "get", "dict_set", "keys", "values", "items", "has_key", "delete_key", "merge",
        "slice", "contains", "index_of", "reverse", "sort", "sum", "range",
        "insert", "remove", "count", "unique", "flatten",
        "first", "last", "take", "drop", "shuffle", "choice", "zip",
        "map", "filter", "reduce",
        // String operations
        "substring", "split", "join", "replace", "trim", "to_upper", "to_lower",
        "starts_with", "ends_with", "repeat", "chr", "ord", "bytes_to_string", "ws_parse_frame_native", "ws_create_frame_native",
        "capitalize", "title", "ltrim", "rtrim", "find",
        "is_alpha", "is_digit", "is_alnum", "is_space", "is_lower", "is_upper",
        "pad_left", "pad_right",
        // Date/time - basic
        "now", "unix_time", "date_format", "date_parse",
        // Date/time - components
        "year", "month", "day", "hour", "minute", "second", "millisecond",
        "weekday", "day_of_year", "week_of_year", "quarter",
        // Date/time - utilities
        "days_in_month", "is_leap_year", "is_weekend", "is_today", "is_same_day",
        // Date/time - timezone
        "timezone", "utc_offset", "set_timezone", "get_timezone",
        // Date/time - arithmetic
        "add_seconds", "add_minutes", "add_hours", "add_days", "add_months", "add_years",
        // Date/time - difference
        "diff_seconds", "diff_days",
        // Date/time - boundaries
        "start_of_day", "end_of_day", "start_of_month", "end_of_month",
        // Network
        "tcp_connect", "tcp_listen", "tcp_accept", "tcp_send", "tcp_recv", "tcp_close",
        "tcp_set_nonblocking", "tcp_has_data", "tcp_select", "tcp_accept_nonblocking", "tcp_recv_nonblocking",
        "iocp_register", "iocp_wait",
        "udp_socket", "udp_bind", "udp_send", "udp_recv", "udp_close",
        // TLS/SSL
        "tls_connect", "tls_listen", "tls_accept", "tls_send", "tls_recv", "tls_recv_all", "tls_close",
        "tls_set_verify", "tls_set_hostname", "tls_get_peer_cert", "tls_get_cipher", "tls_get_version",
        "tls_load_cert", "tls_load_key", "tls_load_ca", "tls_cert_info",
        "tls_wrap_client", "tls_wrap_server", "tls_init", "tls_cleanup",
        // DLL
        "dll_load", "dll_close", "dll_func",
        "dll_call_int", "dll_call_double", "dll_call_str", "dll_call_void",
        "alloc_str", "free_str", "ptr_to_str",
        // Memory read/write
        "read_ptr", "read_int32", "write_ptr", "write_int32",
        // Coroutine (goroutine-style) - moon keyword now uses coroutines
        "yield", "num_goroutines", "num_cpu", "wait_all",
        // Sync operations (Go-style sync package)
        "sync_counter", "sync_add", "sync_get", "sync_set", "sync_cas",
        "sync_mutex", "sync_lock", "sync_unlock", "sync_trylock", "sync_free",
        // Timer
        "set_timeout", "set_interval", "clear_timer",
        // Channel
        "chan", "chan_close",
        // GUI (basic)
        "gui_init", "gui_create", "gui_show", "gui_set_title",
        "gui_set_size", "gui_set_position", "gui_close",
        "gui_run", "gui_quit", "gui_alert", "gui_confirm",
        // GUI (advanced)
        "gui_tray_create", "gui_tray_remove", "gui_tray_set_menu", "gui_tray_on_click",
        "gui_load_url", "gui_load_html", "gui_on_message",
        // GUI (multi-window)
        "gui_load_html", "gui_load_url", "gui_show", "gui_set_title",
        "gui_set_size", "gui_set_position", "gui_close",
        "gui_minimize", "gui_maximize", "gui_restore",
        "gui_on_message", "gui_on_close", "gui_eval", "gui_post_message", "gui_expose"
    };
    return builtins.count(mappedName) > 0;
}

Value* LLVMCodeGen::generateBuiltinCall(const std::string& name, const std::vector<ExprPtr>& args) {
    // Apply alias mapping if available
    std::string funcName = name;
    if (aliasMap && aliasMap->isLoaded()) {
        funcName = aliasMap->mapBuiltin(name);
    }
    
    // Handle print specially - variadic
    if (funcName == "print") {
        int argc = args.size();
        Value* argsArray = builder->CreateAlloca(moonValuePtrType,
            ConstantInt::get(Type::getInt32Ty(*context), argc));
        
        std::vector<Value*> argVals;
        for (int i = 0; i < argc; i++) {
            Value* val = generateExpression(args[i]);
            argVals.push_back(val);
            Value* ptr = builder->CreateGEP(moonValuePtrType, argsArray,
                ConstantInt::get(Type::getInt32Ty(*context), i));
            builder->CreateStore(val, ptr);
        }
        
        builder->CreateCall(getRuntimeFunction("moon_print"),
            {argsArray, ConstantInt::get(Type::getInt32Ty(*context), argc)});
        
        // Release args
        for (auto& val : argVals) {
            builder->CreateCall(getRuntimeFunction("moon_release"), {val});
        }
        
        return generateNullLiteral();
    }
    
    // Type conversion
    if (funcName == "str") {
        Value* arg = generateExpression(args[0]);
        Value* result = builder->CreateCall(getRuntimeFunction("moon_cast_string"), {arg});
        builder->CreateCall(getRuntimeFunction("moon_release"), {arg});
        return result;
    }
    if (funcName == "int") {
        Value* arg = generateExpression(args[0]);
        Value* result = builder->CreateCall(getRuntimeFunction("moon_cast_int"), {arg});
        builder->CreateCall(getRuntimeFunction("moon_release"), {arg});
        return result;
    }
    if (funcName == "float") {
        Value* arg = generateExpression(args[0]);
        Value* result = builder->CreateCall(getRuntimeFunction("moon_cast_float"), {arg});
        builder->CreateCall(getRuntimeFunction("moon_release"), {arg});
        return result;
    }
    
    // Map function names to runtime functions
    std::map<std::string, std::string> funcMap = {
        // I/O and type
        {"input", "moon_input"},
        {"type", "moon_type"},
        {"len", "moon_len"},
        // Math basics
        {"abs", "moon_abs"},
        {"power", "moon_power"},
        {"pow", "moon_power"},
        {"sqrt", "moon_sqrt"},
        {"random_int", "moon_random_int"},
        {"random_float", "moon_random_float"},
        {"random", "moon_random_float"},
        // Math trig
        {"sin", "moon_sin"},
        {"cos", "moon_cos"},
        {"tan", "moon_tan"},
        {"asin", "moon_asin"},
        {"acos", "moon_acos"},
        {"atan", "moon_atan"},
        {"atan2", "moon_atan2"},
        {"sinh", "moon_sinh"},
        {"cosh", "moon_cosh"},
        {"tanh", "moon_tanh"},
        {"hypot", "moon_hypot"},
        // Math other
        {"floor", "moon_floor"},
        {"ceil", "moon_ceil"},
        {"round", "moon_round"},
        {"log", "moon_log"},
        {"log10", "moon_log10"},
        {"log2", "moon_log2"},
        {"exp", "moon_exp"},
        {"degrees", "moon_degrees"},
        {"radians", "moon_radians"},
        {"clamp", "moon_clamp"},
        {"lerp", "moon_lerp"},
        {"sign", "moon_sign"},
        {"mean", "moon_mean"},
        {"median", "moon_median"},
        // System
        {"time", "moon_time"},
        {"sleep", "moon_sleep"},
        {"shell", "moon_shell"},
        {"shell_output", "moon_shell_output"},
        {"env", "moon_env"},
        {"set_env", "moon_set_env"},
        {"exit", "moon_exit"},
        {"argv", "moon_argv"},
        {"platform", "moon_platform"},
        {"getpid", "moon_getpid"},
        {"system", "moon_system"},
        {"exec", "moon_exec"},
        // Memory management (MCU/Embedded)
        {"mem_stats", "moon_mem_stats"},
        {"mem_reset", "moon_mem_reset"},
        {"target_info", "moon_target_info"},
        // GC (Garbage Collection - Cycle Detection)
        {"gc_collect", "gc_collect"},
        {"gc_enable", "gc_enable_val"},
        {"gc_set_threshold", "gc_set_threshold_val"},
        {"gc_stats", "gc_stats"},
        {"gc_debug", "gc_set_debug_val"},
        // Files
        {"read_file", "moon_read_file"},
        {"write_file", "moon_write_file"},
        {"append_file", "moon_append_file"},
        {"exists", "moon_exists"},
        {"is_file", "moon_is_file"},
        {"is_dir", "moon_is_dir"},
        {"list_dir", "moon_list_dir"},
        {"create_dir", "moon_create_dir"},
        {"file_size", "moon_file_size"},
        {"getcwd", "moon_getcwd"},
        {"cd", "moon_cd"},
        {"join_path", "moon_join_path"},
        {"basename", "moon_basename"},
        {"dirname", "moon_dirname"},
        {"extension", "moon_extension"},
        {"absolute_path", "moon_absolute_path"},
        {"copy_file", "moon_copy_file"},
        {"move_file", "moon_move_file"},
        {"remove_file", "moon_remove_file"},
        {"remove_dir", "moon_remove_dir"},
        // String encryption
        {"decrypt_string", "moon_decrypt_string"},
        // JSON
        {"json_encode", "moon_json_encode"},
        {"json_decode", "moon_json_decode"},
        // Regular expressions
        {"regex_match", "moon_regex_match"},
        {"regex_search", "moon_regex_search"},
        {"regex_test", "moon_regex_test"},
        {"regex_groups", "moon_regex_groups"},
        {"regex_named", "moon_regex_named"},
        {"regex_find_all", "moon_regex_find_all"},
        {"regex_find_all_groups", "moon_regex_find_all_groups"},
        {"regex_replace", "moon_regex_replace"},
        {"regex_replace_all", "moon_regex_replace_all"},
        {"regex_split", "moon_regex_split"},
        {"regex_split_n", "moon_regex_split_n"},
        {"regex_compile", "moon_regex_compile"},
        {"regex_match_compiled", "moon_regex_match_compiled"},
        {"regex_search_compiled", "moon_regex_search_compiled"},
        {"regex_find_all_compiled", "moon_regex_find_all_compiled"},
        {"regex_replace_compiled", "moon_regex_replace_compiled"},
        {"regex_free", "moon_regex_free"},
        {"regex_escape", "moon_regex_escape"},
        {"regex_error", "moon_regex_error"},
        // List/Dict operations
        {"append", "moon_list_append"},
        {"pop", "moon_list_pop"},
        {"get", "moon_dict_get"},
        {"dict_set", "moon_dict_set"},
        {"keys", "moon_dict_keys"},
        {"values", "moon_dict_values"},
        {"items", "moon_dict_items"},
        {"has_key", "moon_dict_has_key"},
        {"delete_key", "moon_dict_delete"},
        {"merge", "moon_dict_merge"},
        {"slice", "moon_list_slice"},
        {"contains", "moon_list_contains"},
        {"index_of", "moon_list_index_of"},
        {"reverse", "moon_list_reverse"},
        {"sort", "moon_list_sort"},
        {"sum", "moon_list_sum"},
        {"insert", "moon_list_insert"},
        {"remove", "moon_list_remove"},
        {"count", "moon_list_count"},
        {"unique", "moon_list_unique"},
        {"flatten", "moon_list_flatten"},
        {"first", "moon_list_first"},
        {"last", "moon_list_last"},
        {"take", "moon_list_take"},
        {"drop", "moon_list_drop"},
        {"shuffle", "moon_list_shuffle"},
        {"choice", "moon_list_choice"},
        {"zip", "moon_list_zip"},
        {"map", "moon_list_map"},
        {"filter", "moon_list_filter"},
        {"reduce", "moon_list_reduce"},
        // String operations
        {"substring", "moon_str_substring"},
        {"split", "moon_str_split"},
        {"join", "moon_str_join"},
        {"replace", "moon_str_replace"},
        {"trim", "moon_str_trim"},
        {"to_upper", "moon_str_upper"},
        {"to_lower", "moon_str_lower"},
        {"starts_with", "moon_str_starts_with"},
        {"ends_with", "moon_str_ends_with"},
        {"repeat", "moon_str_repeat"},
        {"chr", "moon_chr"},
        {"ord", "moon_ord"},
        {"bytes_to_string", "moon_bytes_to_string"},
        {"ws_parse_frame_native", "moon_ws_parse_frame"},
        {"ws_create_frame_native", "moon_ws_create_frame"},
        {"capitalize", "moon_str_capitalize"},
        {"title", "moon_str_title"},
        {"ltrim", "moon_str_ltrim"},
        {"rtrim", "moon_str_rtrim"},
        {"find", "moon_str_find"},
        {"is_alpha", "moon_str_is_alpha"},
        {"is_digit", "moon_str_is_digit"},
        {"is_alnum", "moon_str_is_alnum"},
        {"is_space", "moon_str_is_space"},
        {"is_lower", "moon_str_is_lower"},
        {"is_upper", "moon_str_is_upper"},
        {"pad_left", "moon_str_pad_left"},
        {"pad_right", "moon_str_pad_right"},
        // Date/time - basic
        {"now", "moon_now"},
        {"unix_time", "moon_unix_time"},
        {"date_format", "moon_date_format"},
        {"date_parse", "moon_date_parse"},
        // Date/time - components
        {"year", "moon_year"},
        {"month", "moon_month"},
        {"day", "moon_day"},
        {"hour", "moon_hour"},
        {"minute", "moon_minute"},
        {"second", "moon_second"},
        {"millisecond", "moon_millisecond"},
        {"weekday", "moon_weekday"},
        {"day_of_year", "moon_day_of_year"},
        {"week_of_year", "moon_week_of_year"},
        {"quarter", "moon_quarter"},
        // Date/time - utilities
        {"days_in_month", "moon_days_in_month"},
        {"is_leap_year", "moon_is_leap_year"},
        {"is_weekend", "moon_is_weekend"},
        {"is_today", "moon_is_today"},
        {"is_same_day", "moon_is_same_day"},
        // Date/time - timezone
        {"timezone", "moon_timezone"},
        {"utc_offset", "moon_utc_offset"},
        {"set_timezone", "moon_set_timezone"},
        {"get_timezone", "moon_get_timezone"},
        // Date/time - arithmetic
        {"add_seconds", "moon_add_seconds"},
        {"add_minutes", "moon_add_minutes"},
        {"add_hours", "moon_add_hours"},
        {"add_days", "moon_add_days"},
        {"add_months", "moon_add_months"},
        {"add_years", "moon_add_years"},
        // Date/time - difference
        {"diff_seconds", "moon_diff_seconds"},
        {"diff_days", "moon_diff_days"},
        // Date/time - boundaries
        {"start_of_day", "moon_start_of_day"},
        {"end_of_day", "moon_end_of_day"},
        {"start_of_month", "moon_start_of_month"},
        {"end_of_month", "moon_end_of_month"},
        // Network
        {"tcp_connect", "moon_tcp_connect"},
        {"tcp_listen", "moon_tcp_listen"},
        {"tcp_accept", "moon_tcp_accept"},
        {"tcp_send", "moon_tcp_send"},
        {"tcp_recv", "moon_tcp_recv"},
        {"tcp_close", "moon_tcp_close"},
        // Async I/O
        {"tcp_set_nonblocking", "moon_tcp_set_nonblocking"},
        {"tcp_has_data", "moon_tcp_has_data"},
        {"tcp_select", "moon_tcp_select"},
        {"tcp_accept_nonblocking", "moon_tcp_accept_nonblocking"},
        {"tcp_recv_nonblocking", "moon_tcp_recv_nonblocking"},
        // IOCP (Windows high-concurrency)
        {"iocp_register", "moon_iocp_register"},
        {"iocp_wait", "moon_iocp_wait"},
        {"udp_socket", "moon_udp_socket"},
        {"udp_bind", "moon_udp_bind"},
        {"udp_send", "moon_udp_send"},
        {"udp_recv", "moon_udp_recv"},
        {"udp_close", "moon_udp_close"},
        // TLS/SSL
        {"tls_connect", "moon_tls_connect"},
        {"tls_listen", "moon_tls_listen"},
        {"tls_accept", "moon_tls_accept"},
        {"tls_send", "moon_tls_send"},
        {"tls_recv", "moon_tls_recv"},
        {"tls_recv_all", "moon_tls_recv_all"},
        {"tls_set_verify", "moon_tls_set_verify"},
        {"tls_set_hostname", "moon_tls_set_hostname"},
        {"tls_get_peer_cert", "moon_tls_get_peer_cert"},
        {"tls_get_cipher", "moon_tls_get_cipher"},
        {"tls_get_version", "moon_tls_get_version"},
        {"tls_load_cert", "moon_tls_load_cert"},
        {"tls_load_key", "moon_tls_load_key"},
        {"tls_load_ca", "moon_tls_load_ca"},
        {"tls_cert_info", "moon_tls_cert_info"},
        {"tls_wrap_client", "moon_tls_wrap_client"},
        {"tls_wrap_server", "moon_tls_wrap_server"},
        {"tls_init", "moon_tls_init"},
        // DLL
        {"dll_load", "moon_dll_load"},
        {"dll_close", "moon_dll_close"},
        {"dll_func", "moon_dll_func"},
        {"alloc_str", "moon_alloc_str"},
        {"free_str", "moon_free_str"},
        {"ptr_to_str", "moon_ptr_to_str"},
        // Memory read/write
        {"read_ptr", "moon_read_ptr"},
        {"read_int32", "moon_read_int32"},
        {"write_ptr", "moon_write_ptr"},
        {"write_int32", "moon_write_int32"},
        // Coroutine (goroutine-style)
        {"yield", "moon_yield"},
        {"num_goroutines", "moon_num_goroutines"},
        {"num_cpu", "moon_num_cpu"},
        {"wait_all", "moon_wait_all"},
        // Sync operations (Go-style sync package)
        {"sync_counter", "moon_atomic_counter"},
        {"sync_add", "moon_atomic_add"},
        {"sync_get", "moon_atomic_get"},
        {"sync_set", "moon_atomic_set"},
        {"sync_cas", "moon_atomic_cas"},
        {"sync_mutex", "moon_mutex"},
        {"sync_lock", "moon_lock"},
        {"sync_unlock", "moon_unlock"},
        {"sync_trylock", "moon_trylock"},
        {"sync_free", "moon_mutex_free"},
        // HAL - GPIO
        {"gpio_init", "moon_gpio_init"},
        {"gpio_write", "moon_gpio_write"},
        {"gpio_read", "moon_gpio_read"},
        // HAL - PWM
        {"pwm_init", "moon_pwm_init"},
        {"pwm_write", "moon_pwm_write"},
        // HAL - ADC
        {"adc_init", "moon_adc_init"},
        {"adc_read", "moon_adc_read"},
        // HAL - I2C
        {"i2c_init", "moon_i2c_init"},
        {"i2c_write", "moon_i2c_write"},
        {"i2c_read", "moon_i2c_read"},
        // HAL - SPI
        {"spi_init", "moon_spi_init"},
        {"spi_transfer", "moon_spi_transfer"},
        // HAL - UART
        {"uart_init", "moon_uart_init"},
        {"uart_write", "moon_uart_write"},
        {"uart_read", "moon_uart_read"},
        {"uart_available", "moon_uart_available"},
        // HAL - Delay/Timer
        {"millis", "moon_millis"},
        {"micros", "moon_micros"},
        // HAL - System
        {"hal_init", "moon_hal_init_runtime"},
        {"hal_platform", "moon_hal_platform_name"},
        {"hal_debug", "moon_hal_debug_print"}
    };
    
    // Date/time functions with optional timezone parameter (1 required arg + optional tz)
    // year(ts), year(ts, "utc"), month(ts), month(ts, "utc"), etc.
    static std::set<std::string> datetimeFuncsWithTz = {
        "year", "month", "day", "hour", "minute", "second", "weekday",
        "day_of_year", "week_of_year", "quarter",
        "is_weekend", "is_today",
        "start_of_day", "end_of_day", "start_of_month", "end_of_month"
    };
    
    if (datetimeFuncsWithTz.count(name)) {
        std::string rtFunc = funcMap[funcName];
        std::vector<Value*> argVals;
        
        // First arg: timestamp
        argVals.push_back(generateExpression(args[0]));
        
        // Second arg: timezone (optional, default to null for local time)
        if (args.size() >= 2) {
            argVals.push_back(generateExpression(args[1]));
        } else {
            argVals.push_back(generateNullLiteral());
        }
        
        Value* result = builder->CreateCall(getRuntimeFunction(rtFunc), argVals);
        
        // Release args
        for (auto& val : argVals) {
            builder->CreateCall(getRuntimeFunction("moon_release"), {val});
        }
        
        return result;
    }
    
    // is_same_day with optional timezone parameter (2 required args + optional tz)
    if (funcName == "is_same_day") {
        std::vector<Value*> argVals;
        
        argVals.push_back(generateExpression(args[0]));
        argVals.push_back(generateExpression(args[1]));
        
        if (args.size() >= 3) {
            argVals.push_back(generateExpression(args[2]));
        } else {
            argVals.push_back(generateNullLiteral());
        }
        
        Value* result = builder->CreateCall(getRuntimeFunction("moon_is_same_day"), argVals);
        
        for (auto& val : argVals) {
            builder->CreateCall(getRuntimeFunction("moon_release"), {val});
        }
        
        return result;
    }
    
    // date_format with optional timezone parameter
    // date_format(ts, fmt), date_format(ts, fmt, "utc")
    if (funcName == "date_format") {
        std::vector<Value*> argVals;
        
        // First arg: timestamp
        argVals.push_back(generateExpression(args[0]));
        
        // Second arg: format (optional, but usually provided)
        if (args.size() >= 2) {
            argVals.push_back(generateExpression(args[1]));
        } else {
            argVals.push_back(generateNullLiteral());
        }
        
        // Third arg: timezone (optional, default to null for local time)
        if (args.size() >= 3) {
            argVals.push_back(generateExpression(args[2]));
        } else {
            argVals.push_back(generateNullLiteral());
        }
        
        Value* result = builder->CreateCall(getRuntimeFunction("moon_date_format"), argVals);
        
        // Release args
        for (auto& val : argVals) {
            builder->CreateCall(getRuntimeFunction("moon_release"), {val});
        }
        
        return result;
    }
    
    if (funcMap.count(funcName)) {
        std::string rtFunc = funcMap[funcName];
        std::vector<Value*> argVals;
        for (const auto& arg : args) {
            argVals.push_back(generateExpression(arg));
        }
        
        Function* func = getRuntimeFunction(rtFunc);
        if (!func) {
            setError("Runtime function not found: " + rtFunc);
            return generateNullLiteral();
        }
        
        // Check if function returns void
        bool isVoidFunc = func->getReturnType()->isVoidTy();
        
        Value* result = builder->CreateCall(func, argVals);
        
        // Release args
        for (auto& val : argVals) {
            builder->CreateCall(getRuntimeFunction("moon_release"), {val});
        }
        
        // For void functions, return null instead of the void instruction
        if (isVoidFunc) {
            return generateNullLiteral();
        }
        
        return result ? result : generateNullLiteral();
    }
    
    // Variadic functions (min, max, format, range, dll_call_*)
    if (funcName == "min" || name == "max" || name == "format" || name == "range" ||
        name == "dll_call_int" || name == "dll_call_double" || name == "dll_call_str" || name == "dll_call_void") {
        int argc = args.size();
        
        // For dll_call_*, first arg is the function pointer
        if (funcName == "dll_call_int" || name == "dll_call_double" || 
            name == "dll_call_str" || name == "dll_call_void") {
            Value* funcPtr = generateExpression(args[0]);
            int numArgs = argc - 1;
            
            Value* argsArray = builder->CreateAlloca(moonValuePtrType,
                ConstantInt::get(Type::getInt32Ty(*context), numArgs));
            
            std::vector<Value*> argVals;
            for (int i = 1; i < argc; i++) {
                Value* val = generateExpression(args[i]);
                argVals.push_back(val);
                Value* ptr = builder->CreateGEP(moonValuePtrType, argsArray,
                    ConstantInt::get(Type::getInt32Ty(*context), i - 1));
                builder->CreateStore(val, ptr);
            }
            
            std::string rtFunc = "moon_" + name;
            Value* result;
            if (funcName == "dll_call_void") {
                builder->CreateCall(getRuntimeFunction(rtFunc),
                    {funcPtr, argsArray, ConstantInt::get(Type::getInt32Ty(*context), numArgs)});
                result = generateNullLiteral();
            } else {
                result = builder->CreateCall(getRuntimeFunction(rtFunc),
                    {funcPtr, argsArray, ConstantInt::get(Type::getInt32Ty(*context), numArgs)});
            }
            
            for (auto& val : argVals) {
                builder->CreateCall(getRuntimeFunction("moon_release"), {val});
            }
            builder->CreateCall(getRuntimeFunction("moon_release"), {funcPtr});
            
            return result;
        }
        
        // Regular variadic functions
        Value* argsArray = builder->CreateAlloca(moonValuePtrType,
            ConstantInt::get(Type::getInt32Ty(*context), argc));
        
        std::vector<Value*> argVals;
        for (int i = 0; i < argc; i++) {
            Value* val = generateExpression(args[i]);
            argVals.push_back(val);
            Value* ptr = builder->CreateGEP(moonValuePtrType, argsArray,
                ConstantInt::get(Type::getInt32Ty(*context), i));
            builder->CreateStore(val, ptr);
        }
        
        std::string rtFunc = "moon_" + name;
        Value* result = builder->CreateCall(getRuntimeFunction(rtFunc),
            {argsArray, ConstantInt::get(Type::getInt32Ty(*context), argc)});
        
        for (auto& val : argVals) {
            builder->CreateCall(getRuntimeFunction("moon_release"), {val});
        }
        
        return result;
    }
    
    // Void functions (1 arg)
    if (funcName == "tcp_close" || name == "udp_close" || name == "dll_close" || 
        name == "free_str" || name == "clear_timer" || name == "chan_close" ||
        name == "gui_close" || name == "gui_quit" ||
        // TLS functions
        name == "tls_close" ||
        // HAL deinit functions
        name == "gpio_deinit" || name == "pwm_deinit" || name == "adc_deinit" ||
        name == "i2c_deinit" || name == "spi_deinit" || name == "uart_deinit") {
        if (args.size() >= 1) {
            Value* arg = generateExpression(args[0]);
            builder->CreateCall(getRuntimeFunction("moon_" + name), {arg});
            builder->CreateCall(getRuntimeFunction("moon_release"), {arg});
        } else if (funcName == "gui_quit") {
            builder->CreateCall(getRuntimeFunction("moon_gui_quit"), {});
        }
        return generateNullLiteral();
    }
    
    // HAL delay functions (1 arg, void return)
    if (funcName == "delay_ms" || name == "delay_us") {
        Value* arg = generateExpression(args[0]);
        builder->CreateCall(getRuntimeFunction("moon_" + name), {arg});
        builder->CreateCall(getRuntimeFunction("moon_release"), {arg});
        return generateNullLiteral();
    }
    
    // HAL deinit (0 args)
    if (funcName == "hal_deinit") {
        builder->CreateCall(getRuntimeFunction("moon_hal_deinit_runtime"), {});
        return generateNullLiteral();
    }
    
    // TLS cleanup (0 args)
    if (funcName == "tls_cleanup") {
        builder->CreateCall(getRuntimeFunction("moon_tls_cleanup"), {});
        return generateNullLiteral();
    }
    
    // Timer functions
    if (funcName == "set_timeout" || name == "set_interval") {
        Value* callback = generateExpression(args[0]);
        Value* ms = generateExpression(args[1]);
        Value* result = builder->CreateCall(getRuntimeFunction("moon_" + name), {callback, ms});
        builder->CreateCall(getRuntimeFunction("moon_release"), {callback});
        builder->CreateCall(getRuntimeFunction("moon_release"), {ms});
        return result;
    }
    
    // Channel function
    if (funcName == "chan") {
        int argc = args.size();
        Value* argsArray = builder->CreateAlloca(moonValuePtrType,
            ConstantInt::get(Type::getInt32Ty(*context), argc));
        
        std::vector<Value*> argVals;
        for (int i = 0; i < argc; i++) {
            Value* val = generateExpression(args[i]);
            argVals.push_back(val);
            Value* ptr = builder->CreateGEP(moonValuePtrType, argsArray,
                ConstantInt::get(Type::getInt32Ty(*context), i));
            builder->CreateStore(val, ptr);
        }
        
        Value* result = builder->CreateCall(getRuntimeFunction("moon_chan"),
            {argsArray, ConstantInt::get(Type::getInt32Ty(*context), argc)});
        
        for (auto& val : argVals) {
            builder->CreateCall(getRuntimeFunction("moon_release"), {val});
        }
        return result;
    }
    
    // GUI functions - set flag for WINDOWS subsystem
    if (funcName.substr(0, 4) == "gui_") {
        usesGUI = true;
    }
    
    // Network functions - set flag for network libraries
    if (name.substr(0, 4) == "tcp_" || name.substr(0, 4) == "udp_") {
        usesNetwork = true;
    }
    
    // TLS functions - set flag for crypto libraries (crypt32.lib on Windows)
    if (name.substr(0, 4) == "tls_") {
        usesTLS = true;
        usesNetwork = true;  // TLS also needs ws2_32.lib
    }
    
    // GUI functions (0 args)
    if (funcName == "gui_init" || funcName == "gui_run") {
        if (funcName == "gui_init") {
            return builder->CreateCall(getRuntimeFunction("moon_gui_init"), {});
        } else {
            builder->CreateCall(getRuntimeFunction("moon_gui_run"), {});
            return generateNullLiteral();
        }
    }
    
    // GUI functions (1 arg) - but NOT gui_create with 4 args
    if ((funcName == "gui_create" && args.size() == 1) || funcName == "gui_alert" || funcName == "gui_confirm") {
        Value* arg = generateExpression(args[0]);
        Value* result = builder->CreateCall(getRuntimeFunction("moon_" + funcName), {arg});
        builder->CreateCall(getRuntimeFunction("moon_release"), {arg});
        return result;
    }
    
    // gui_create(title, width, height, options) - 4 args.
    // Keep this explicit branch before other GUI overloads to avoid mismatched dispatch.
    if (funcName == "gui_create" && args.size() == 4) {
        Value* arg1 = generateExpression(args[0]);
        Value* arg2 = generateExpression(args[1]);
        Value* arg3 = generateExpression(args[2]);
        Value* arg4 = generateExpression(args[3]);
        Value* result = builder->CreateCall(getRuntimeFunction("moon_gui_create_advanced"), 
                                            {arg1, arg2, arg3, arg4});
        builder->CreateCall(getRuntimeFunction("moon_release"), {arg1});
        builder->CreateCall(getRuntimeFunction("moon_release"), {arg2});
        builder->CreateCall(getRuntimeFunction("moon_release"), {arg3});
        builder->CreateCall(getRuntimeFunction("moon_release"), {arg4});
        return result;
    }
    
    // gui_show: 1 arg (legacy bool) or 2 args (winId, bool)
    if (funcName == "gui_show") {
        if (args.size() == 2) {
            Value* winId = generateExpression(args[0]);
            Value* show = generateExpression(args[1]);
            builder->CreateCall(getRuntimeFunction("moon_gui_show_win"), {winId, show});
            builder->CreateCall(getRuntimeFunction("moon_release"), {winId});
            builder->CreateCall(getRuntimeFunction("moon_release"), {show});
        } else {
            Value* arg = generateExpression(args[0]);
            builder->CreateCall(getRuntimeFunction("moon_gui_show_window"), {arg});
            builder->CreateCall(getRuntimeFunction("moon_release"), {arg});
        }
        return generateNullLiteral();
    }
    
    // gui_set_title: 1 arg (legacy) or 2 args (winId, title)
    if (funcName == "gui_set_title") {
        if (args.size() == 2) {
            Value* winId = generateExpression(args[0]);
            Value* title = generateExpression(args[1]);
            builder->CreateCall(getRuntimeFunction("moon_gui_set_title_win"), {winId, title});
            builder->CreateCall(getRuntimeFunction("moon_release"), {winId});
            builder->CreateCall(getRuntimeFunction("moon_release"), {title});
        } else {
            // Legacy: single arg is title
            Value* arg = generateExpression(args[0]);
            builder->CreateCall(getRuntimeFunction("moon_gui_set_title"), {arg, arg});
            builder->CreateCall(getRuntimeFunction("moon_release"), {arg});
        }
        return generateNullLiteral();
    }
    
    // gui_set_size: 2 args (legacy w,h) or 3 args (winId, w, h)
    if (funcName == "gui_set_size") {
        if (args.size() == 3) {
            Value* winId = generateExpression(args[0]);
            Value* w = generateExpression(args[1]);
            Value* h = generateExpression(args[2]);
            builder->CreateCall(getRuntimeFunction("moon_gui_set_size_win"), {winId, w, h});
            builder->CreateCall(getRuntimeFunction("moon_release"), {winId});
            builder->CreateCall(getRuntimeFunction("moon_release"), {w});
            builder->CreateCall(getRuntimeFunction("moon_release"), {h});
        } else {
            Value* w = generateExpression(args[0]);
            Value* h = generateExpression(args[1]);
            builder->CreateCall(getRuntimeFunction("moon_gui_set_size"), {w, w, h});
            builder->CreateCall(getRuntimeFunction("moon_release"), {w});
            builder->CreateCall(getRuntimeFunction("moon_release"), {h});
        }
        return generateNullLiteral();
    }
    
    // gui_set_position: 2 args (legacy x,y) or 3 args (winId, x, y)
    if (funcName == "gui_set_position") {
        if (args.size() == 3) {
            Value* winId = generateExpression(args[0]);
            Value* x = generateExpression(args[1]);
            Value* y = generateExpression(args[2]);
            builder->CreateCall(getRuntimeFunction("moon_gui_set_position_win"), {winId, x, y});
            builder->CreateCall(getRuntimeFunction("moon_release"), {winId});
            builder->CreateCall(getRuntimeFunction("moon_release"), {x});
            builder->CreateCall(getRuntimeFunction("moon_release"), {y});
        } else {
            Value* x = generateExpression(args[0]);
            Value* y = generateExpression(args[1]);
            builder->CreateCall(getRuntimeFunction("moon_gui_set_position"), {x, x, y});
            builder->CreateCall(getRuntimeFunction("moon_release"), {x});
            builder->CreateCall(getRuntimeFunction("moon_release"), {y});
        }
        return generateNullLiteral();
    }
    
    // gui_close: 0 args (legacy) or 1 arg (winId)
    if (funcName == "gui_close") {
        if (args.size() >= 1) {
            Value* winId = generateExpression(args[0]);
            builder->CreateCall(getRuntimeFunction("moon_gui_close_win"), {winId});
            builder->CreateCall(getRuntimeFunction("moon_release"), {winId});
        } else {
            builder->CreateCall(getRuntimeFunction("moon_gui_close"), {generateNullLiteral()});
        }
        return generateNullLiteral();
    }
    
    // ============== Advanced GUI Functions ==============
    
    // gui_tray_create(tooltip, iconPath) - 2 args
    if (funcName == "gui_tray_create") {
        Value* arg1 = generateExpression(args[0]);
        Value* arg2 = generateExpression(args[1]);
        Value* result = builder->CreateCall(getRuntimeFunction("moon_gui_tray_create"), {arg1, arg2});
        builder->CreateCall(getRuntimeFunction("moon_release"), {arg1});
        builder->CreateCall(getRuntimeFunction("moon_release"), {arg2});
        return result;
    }
    
    // gui_tray_remove() - 0 args
    if (funcName == "gui_tray_remove") {
        builder->CreateCall(getRuntimeFunction("moon_gui_tray_remove"), {});
        return generateNullLiteral();
    }
    
    // gui_tray_set_menu(items) - 1 arg
    if (funcName == "gui_tray_set_menu") {
        Value* arg = generateExpression(args[0]);
        Value* result = builder->CreateCall(getRuntimeFunction("moon_gui_tray_set_menu"), {arg});
        builder->CreateCall(getRuntimeFunction("moon_release"), {arg});
        return result;
    }
    
    // gui_tray_on_click(callback) - 1 arg
    if (funcName == "gui_tray_on_click") {
        Value* arg = generateExpression(args[0]);
        builder->CreateCall(getRuntimeFunction("moon_gui_tray_on_click"), {arg});
        // Don't release callback - it's stored
        return generateNullLiteral();
    }
    
    // gui_load_url: 1 arg (legacy) or 2 args (with winId)
    if (funcName == "gui_load_url") {
        if (args.size() == 2) {
            Value* winId = generateExpression(args[0]);
            Value* url = generateExpression(args[1]);
            builder->CreateCall(getRuntimeFunction("moon_gui_load_url_win"), {winId, url});
            builder->CreateCall(getRuntimeFunction("moon_release"), {winId});
            builder->CreateCall(getRuntimeFunction("moon_release"), {url});
        } else {
            Value* arg = generateExpression(args[0]);
            builder->CreateCall(getRuntimeFunction("moon_gui_load_url"), {arg});
            builder->CreateCall(getRuntimeFunction("moon_release"), {arg});
        }
        return generateNullLiteral();
    }
    
    // gui_load_html: 1 arg (legacy) or 2 args (with winId)
    if (funcName == "gui_load_html") {
        if (args.size() == 2) {
            Value* winId = generateExpression(args[0]);
            Value* html = generateExpression(args[1]);
            builder->CreateCall(getRuntimeFunction("moon_gui_load_html_win"), {winId, html});
            builder->CreateCall(getRuntimeFunction("moon_release"), {winId});
            builder->CreateCall(getRuntimeFunction("moon_release"), {html});
        } else {
            Value* arg = generateExpression(args[0]);
            builder->CreateCall(getRuntimeFunction("moon_gui_load_html"), {arg});
            builder->CreateCall(getRuntimeFunction("moon_release"), {arg});
        }
        return generateNullLiteral();
    }
    
    // gui_on_message: 1 arg (legacy) or 2 args (with winId)
    if (funcName == "gui_on_message") {
        if (args.size() == 2) {
            Value* winId = generateExpression(args[0]);
            Value* callback = generateExpression(args[1]);
            builder->CreateCall(getRuntimeFunction("moon_gui_on_message_win"), {winId, callback});
            builder->CreateCall(getRuntimeFunction("moon_release"), {winId});
            // Don't release callback - it's stored
        } else {
            Value* arg = generateExpression(args[0]);
            builder->CreateCall(getRuntimeFunction("moon_gui_on_message"), {arg});
            // Don't release callback - it's stored
        }
        return generateNullLiteral();
    }
    
    // gui_on_close(winId, callback) - 2 args
    if (funcName == "gui_on_close" && args.size() == 2) {
        Value* winId = generateExpression(args[0]);
        Value* callback = generateExpression(args[1]);
        builder->CreateCall(getRuntimeFunction("moon_gui_on_close_win"), {winId, callback});
        builder->CreateCall(getRuntimeFunction("moon_release"), {winId});
        return generateNullLiteral();
    }
    
    // gui_eval(winId, js) - 2 args
    if (funcName == "gui_eval" && args.size() == 2) {
        Value* winId = generateExpression(args[0]);
        Value* js = generateExpression(args[1]);
        Value* result = builder->CreateCall(getRuntimeFunction("moon_gui_eval_win"), {winId, js});
        builder->CreateCall(getRuntimeFunction("moon_release"), {winId});
        builder->CreateCall(getRuntimeFunction("moon_release"), {js});
        return result;
    }
    
    // gui_post_message(winId, msg) - 2 args
    if (funcName == "gui_post_message" && args.size() == 2) {
        Value* winId = generateExpression(args[0]);
        Value* msg = generateExpression(args[1]);
        builder->CreateCall(getRuntimeFunction("moon_gui_post_message_win"), {winId, msg});
        builder->CreateCall(getRuntimeFunction("moon_release"), {winId});
        builder->CreateCall(getRuntimeFunction("moon_release"), {msg});
        return generateNullLiteral();
    }
    
    // gui_minimize(winId) - 1 arg
    if (funcName == "gui_minimize" && args.size() == 1) {
        Value* winId = generateExpression(args[0]);
        builder->CreateCall(getRuntimeFunction("moon_gui_minimize_win"), {winId});
        builder->CreateCall(getRuntimeFunction("moon_release"), {winId});
        return generateNullLiteral();
    }
    
    // gui_maximize(winId) - 1 arg
    if (funcName == "gui_maximize" && args.size() == 1) {
        Value* winId = generateExpression(args[0]);
        builder->CreateCall(getRuntimeFunction("moon_gui_maximize_win"), {winId});
        builder->CreateCall(getRuntimeFunction("moon_release"), {winId});
        return generateNullLiteral();
    }
    
    // gui_restore(winId) - 1 arg
    if (funcName == "gui_restore" && args.size() == 1) {
        Value* winId = generateExpression(args[0]);
        builder->CreateCall(getRuntimeFunction("moon_gui_restore_win"), {winId});
        builder->CreateCall(getRuntimeFunction("moon_release"), {winId});
        return generateNullLiteral();
    }
    
    // gui_expose(name, callback) - 2 args (uses first window)
    // gui_expose(winId, name, callback) - 3 args
    if (funcName == "gui_expose") {
        if (args.size() == 2) {
            Value* funcName = generateExpression(args[0]);
            Value* callback = generateExpression(args[1]);
            builder->CreateCall(getRuntimeFunction("moon_gui_expose"), {funcName, callback});
            builder->CreateCall(getRuntimeFunction("moon_release"), {funcName});
            // Don't release callback - it's stored
            return generateNullLiteral();
        } else if (args.size() == 3) {
            Value* winId = generateExpression(args[0]);
            Value* funcName = generateExpression(args[1]);
            Value* callback = generateExpression(args[2]);
            builder->CreateCall(getRuntimeFunction("moon_gui_expose_win"), {winId, funcName, callback});
            builder->CreateCall(getRuntimeFunction("moon_release"), {winId});
            builder->CreateCall(getRuntimeFunction("moon_release"), {funcName});
            // Don't release callback - it's stored
            return generateNullLiteral();
        }
    }
    
    setError("Unknown builtin function: " + name);
    return generateNullLiteral();
}
