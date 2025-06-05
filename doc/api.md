# Sc C API
When using `sc` in a project, a `sc_ctx` must be created first, create it like any other C variable, but ensure it is 0ed out. `sc` manages it's own heap memory which can be configured using `HEAP_SIZE` in `config.h`, ensure you are not setting the heap limit beyond `UINT16_MAX`.
```c
struct sc_ctx ctx = { 0 };
```

## Evaluating code
To evaluate code stored in a string, you can use `sc_eval` function, be aware each time you call `sc_eval` previous state gets wiped and is no longer usabe, it is highly advised to no longer use result value from previous `sc_eval` invocation.
```c
const char *code = "(+ 1 2)";
sc_value result = sc_eval(&ctx, code, strlen(code));

/* here handle possible errors encountered during evaluation
 * or work with the returned value, do not work with it after calling
 * sc_eval again!
 */
```

## Running returned lambda
`sc_eval` is able to return value of type `SC_LAMBDA_VAL`, you can call this function using `sc_eval_lambda` even after `sc_eval` has finished as the state persists.
```c
const char *code = "(lambda (x) (* x x))";
sc_value lambda = sc_eval(&ctx, code, strlen(code));

if (result.type == SC_LAMBDA_VAL) {
    sc_value args[1];
    args[0] = sc_num(8);
    sc_value result = sc_eval_lambda(&ctx, &lambda, args, 1); /* should return 64 */
}
```

## Providing custom C functions
`sc_ctx` has a field called `user_fns` which takes in an array of `struct sc_fns`, each provided function has a `name`, if it is `lazy` and the function pointer itself. The array must contain last element that is 0ed out.
If `lazy` is set to true, `sc` won't evaluate arguments provided to the function, instead stores their `lazy_addr` which can be then used to lazily process input data.
The function pointer is a type of `sc_fn` which has to match this signature:
```c
sc_value name(struct sc_ctx *, sc_value*, uint16_t);
```
First argument being `sc_ctx` itself, the second argument is pointing at arguments put into the function and last specifies how many arguments were collected.
You can provide custom functions like so:
```c
sc_value add(struct sc_ctx *ctx, sc_value *args, uint16_t nargs)
{
    return sc_num(args[0].number + args[1].number);
}

struct sc_fns funcs[] = {
    { false, "add", add },
    { false, NULL, NULL },
};

struct sc_ctx ctx = { 0 };
ctx.user_fns = funcs;
```
Now the the function can be called like any other function:
```scm
(display (add 1 2))
```

## Error handling
There is not much you can do other than access the error message through `result.err`.
As of now, no stack trace is captured.

## Creating values
`sc` provides APIs to creating primitive values, strings and errors respectively:
- `sc_nil` - a constant to return "nothing"
- `sc_num`
- `sc_real`
- `sc_bool`
- `sc_string`
- `sc_error`

## Persisting values from arguments
After each function call `sc` performs argument clean up. In order to have an argument persist longer, you can use `sc_dup_value` and re-use it when returning a result.
If you happen want to cleanup resources in case of early returning an error, you can cleanup allocated values using `sc_free_value`.

## Custom userdata
To create custom object which is also tracked by the GC, function `sc_userdata` can be used. The last argument of `sc_userdata` can be `NULL` and thus no function will be executed to perform the cleanup.
Custom userdata can be created like so:
```c
void foo_collect(struct sc_ctx *ctx, void *data)
{
    printf("GC was called! value => %d\n", *(int*) data);
}

sc_value foo(struct sc_ctx *ctx, sc_value *args, uint16_t bargs)
{
    sc_value custom = sc_userdata(ctx, sizeof(int), foo_collect);
    *(int*) custom.userdata.data = 77;
    return custom;
}
```

## Allocating custom data
If you want, you can also use `sc_alloc`/`sc_free` to allocate/deallocate memory on `sc` heap. The value however is not tracked so you are required to call `sc_free`, in case you want the value to live longer, you can use `sc_dup` to increase usage count.

## Miscellaneous utilities
You can strictly compare values using `sc_value_eq` (equivalent to `eq?`), display returned result using `sc_display` (equivalent to `display`) and return heap usage in bytes using `sc_heap_usage`.
