%%
%% This file contains the ErLang header definition for global message type.
%% Global message type should be encoded as the first byte of any message in system.
%% This type will signal framework node behaivor.
%%

-ifndef(GLB_MSG_HRL_).
-define(GLB_MSG_HRL_, 1).

%% message definition
-define(ACK, 0).      %% message is ack of incoming command
-define(RESPONSE, 1). %% message is a response to node command
-define(PASS, 2).     %% message is to be passed through node without capturing
-define(COPY, 3).     %% message is to be passed through node with capturing
-define(CRASH, 4).    %% other end is suppose to capture message, pass it along, wait for ack and then kill the other C/C++ node
-define(FAULT, 5).    %% message signals fault detection. connected node should capture and log. all running keep going as normal
-define(TRACE, 6).    %% message used for path tracing. each node on the path should make a copy and append node info at message end
-define(TIMEOUT, 7).  %% message used to signal timeout
-define(INIT, 8).     %% message is for any init trigger from node to external C/C++ program

%% magic pattern for trace message tailing. using 0xaa 0xaa 0xaa for 24 bytes. last 8 byte for special character \.
-define(MAGIC_PATTERN, [170, 170, 170, 92]).

-endif.
