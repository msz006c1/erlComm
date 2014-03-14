-ifndef(GLOBAL_ERR_MSG_H).
-define(GLOBAL_ERR_MSG_H, 1).

-define(NO_ERROR,            0).       %% this is to signal call finished without any issue.
-define(GENERIC_ERROR,      -1).       %% this is to signal for a generic error. most likely out of memory.
-define(SELF_CONTAINED,     -2).       %% this is to signal no respond is needed.
-define(ARG_ERROR,          -3).       %% this is to signal system call has incorrect argument.
-define(NOT_HANDLED,        -4).       %% this is to signal the routine shall not be handled by current call.
-define(IO_ERROR,           -5).       %% this is to signal for an I/O error.
-define(PTHREAD_ERROR,      -6).       %% this is to signal thread operation failure.

-endif. %%GLOBAL_ERR_MSG_H
