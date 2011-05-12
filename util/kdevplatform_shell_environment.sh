#!/bin/sh

# This file is part of KDevelop
# Copyright 2011 David Nolden <david.nolden.kdevelop@art-master.de>
# 
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
# 
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
# 
# You should have received a copy of the GNU Library General Public License
# along with this library; see the file COPYING.LIB.  If not, write to
# the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
# Boston, MA 02110-1301, USA.

source ~/.bashrc

export BASE_SHELL_PROMPT=$PS1

if ! [ "$APPLICATION_HOST" ]; then
    export APPLICATION_HOST=$(hostname)
fi

if ! [ "$KDEV_DBUS_ID" ]; then
    echo "The required environment variable KDEV_DBUS_ID is not set. This variable defines the dbus id of the application instance instance which is supposed to be attached."
    exit 5
fi

function getSessionName {
    echo "$(qdbus $KDEV_DBUS_ID /kdevelop/SessionController org.kdevelop.kdevelop.KDevelop.SessionController.sessionName)"
}

function updateShellPrompt {
    # Makes the shell prompt show the current name of this session
#     SESSION_NAME=$(getSessionName)
    PS1="!$BASE_SHELL_PROMPT" # Just somehow set a mark that this session is attached
}

function help {
    echo "You are controlling the $APPLICATION session '$(getSessionName)'"
    echo ""
    echo "Available commands:"
    echo "raise                               - Raise the window"
    echo "open [file] ...                     - Open the files, referenced by relative or absolute paths"
    echo "create [file] [[text]]              - Create and open a new file with the given optional text"
    echo "search [pattern] [[locations]] ...  - Search for the given pattern here or at the optionally given location(s)."
    echo "dsearch [pattern] [[locations]] ... - Same as search, but starts the search instantly instead of showing the dialog."
    echo "fssh [ssh arguments]                - Connect to a remote host via ssh, keeping the control-connection alive."
    echo "                                    - The whole dbus environment will be forwarded, KDevelop needs to be installed on both sides."
    echo ""
}

function openDocument {
    RESULT=$(qdbus $KDEV_DBUS_ID /org/kdevelop/DocumentController org.kdevelop.DocumentController.openDocumentSimple $1)
    if ! [ "$RESULT" == "true" ]; then
        echo "Failed to open $1"
    fi
}

# Main functions:

function raise {
    qdbus $KDEV_DBUS_ID /kdevelop/MainWindow org.kdevelop.MainWindow.ensureVisible
}

function open {
    FILES=$@
    for RELATIVE_FILE in $FILES; do
        # TODO: Support ':linenumber' at the end of the file
        FILE=$(readlink -f $RELATIVE_FILE)
        if ! [ "$FILE" ]; then
            # Try opening the file anyway, it might be an url or something else we don't understand here
            FILE=$RELATIVE_FILE
        fi
        openDocument "$FILE"
    done
}

function create {
    FILE=$(readlink -f $1)
    if ! [ "$FILE" ]; then
        echo "Error: Bad arguments."
        return 1
    fi
    if [ -e "$FILE" ]; then
        echo "The file $FILE already exists"
        return 2
    fi
    echo $2 > $FILE
    openDocument $FILE
}

function search {
    PATTERN=$1
    
    if ! [ "$PATTERN" ]; then
        echo "Error: No pattern given."
        return 1
    fi

    LOCATION=$2

    if ! [ "$LOCATION" ]; then
        LOCATION="."
    fi
    
    LOCATION=$(readlink -f $LOCATION)
    
    for LOC in $*; do
        if [ "$LOC" == "$1" ]; then
            continue;
        fi
        if [ "$LOC" == "$2" ]; then
            continue;
        fi
        LOCATION="$LOCATION;$(readlink -f $LOC)"
    done
    
    qdbus $KDEV_DBUS_ID /org/kdevelop/GrepViewPlugin org.kdevelop.kdevelop.GrepViewPlugin.startSearch "$PATTERN" "$LOCATION" true
}

function dsearch {
    PATTERN=$1
    
    if ! [ "$PATTERN" ]; then
        echo "Error: No pattern given."
        return 1
    fi

    LOCATION=$2

    if ! [ "$LOCATION" ]; then
        LOCATION="."
    fi
    
    LOCATION=$(readlink -f $LOCATION)
    
    for LOC in $*; do
        if [ "$LOC" == "$1" ]; then
            continue;
        fi
        if [ "$LOC" == "$2" ]; then
            continue;
        fi
        LOCATION="$LOCATION;$(readlink -f $LOC)"
    done
    
    qdbus $KDEV_DBUS_ID /org/kdevelop/GrepViewPlugin org.kdevelop.kdevelop.GrepViewPlugin.startSearch "$PATTERN" "$LOCATION" false
}

##### SSH DBUS FORWARDING --------------------------------------------------------------------------------------------------------------------

DBUS_SOCKET_TRANSFORMER=$KDEV_BASEDIR/kdev_dbus_socket_transformer

# We need this, to make sure that our forwarding-loops won't get out of control
# This configures the shell to kill background jobs when it is terminated
shopt -s huponexit

# TODO: This random number can lead to conflicts, but since only ssh notices these conflicts
#       during forwarding, it is very hard to deal with them.
export DBUS_FORWARDING_TCP_TARGET_PORT=$((5000+($RANDOM%50000)))

export DBUS_ABSTRACT_SOCKET_TARGET_BASE_PATH=/tmp/dbus-forwarded-$USER-$APPLICATION_HOST

export DBUS_FORWARDING_TCP_LOCAL_PORT=9000
export DBUS_FORWARDING_TCP_MAX_LOCAL_PORT=10000
export DBUS_ABSTRACT_SOCKET_TARGET_INDEX=1
export DBUS_ABSTRACT_SOCKET_MAX_TARGET_INDEX=1000

function getDBusAbstractSocketSuffix {
    # From something like DBUS_SESSION_BUS_ADDRESS=unix:abstract=/tmp/dbus-wYmSkVH7FE,guid=b214dad39e0292a4299778d64d761a5b
    # extract the /tmp/dbus-wYmSkVH7FE
    echo $DBUS_SESSION_BUS_ADDRESS | sed 's/unix\:abstract\=.*\(,guid\=.*\)/\1/'
}

function keepForwardingDBusToTCPSocket {
    while ! $KDEV_BASEDIR/kdev_dbus_socket_transformer $DBUS_FORWARDING_TCP_LOCAL_PORT --bind-only; do
        if (($DBUS_FORWARDING_TCP_LOCAL_PORT<$DBUS_FORWARDING_TCP_MAX_LOCAL_PORT)); then
            export DBUS_FORWARDING_TCP_LOCAL_PORT=$(($DBUS_FORWARDING_TCP_LOCAL_PORT+1))
#             echo "Increased local port to " $DBUS_FORWARDING_TCP_LOCAL_PORT;
        else
            echo "Failed to allocate a local TCP port";
            return 1;
        fi
    done
        
    $KDEV_BASEDIR/kdev_dbus_socket_transformer $DBUS_FORWARDING_TCP_LOCAL_PORT&
    return 0;
}

function keepForwardingDBusFromTCPSocket {

    while ! $KDEV_BASEDIR/kdev_dbus_socket_transformer $FORWARD_DBUS_FROM_PORT ${DBUS_ABSTRACT_SOCKET_TARGET_BASE_PATH}-${DBUS_ABSTRACT_SOCKET_TARGET_INDEX} --bind-only; do
        if ((${DBUS_ABSTRACT_SOCKET_TARGET_INDEX}<${DBUS_ABSTRACT_SOCKET_MAX_TARGET_INDEX})); then
            export DBUS_ABSTRACT_SOCKET_TARGET_INDEX=$(($DBUS_ABSTRACT_SOCKET_TARGET_INDEX+1))
        else
            echo "Failed to allocate a local path for the abstract dbus socket";
            return 1;
        fi
    done
    
    local PATH=${DBUS_ABSTRACT_SOCKET_TARGET_BASE_PATH}-${DBUS_ABSTRACT_SOCKET_TARGET_INDEX}
    export DBUS_SESSION_BUS_ADDRESS=unix:abstract=$PATH${DBUS_SOCKET_SUFFIX}
    $KDEV_BASEDIR/kdev_dbus_socket_transformer $FORWARD_DBUS_FROM_PORT $PATH&
}

function fssh {
    echo "forwarding from $APPLICATION_HOST"
    keepForwardingDBusToTCPSocket # Should be automatically terminated when the function exits
    echo "calling" ssh $@ -t -R localhost:$DBUS_FORWARDING_TCP_TARGET_PORT:localhost:$DBUS_FORWARDING_TCP_LOCAL_PORT \
           "APPLICATION=$APPLICATION KDEV_BASEDIR=$KDEV_BASEDIR KDEV_DBUS_ID=$KDEV_DBUS_ID FORWARD_DBUS_FROM_PORT=$DBUS_FORWARDING_TCP_TARGET_PORT APPLICATION_HOST=$APPLICATION_HOST DBUS_SOCKET_SUFFIX=$(getDBusAbstractSocketSuffix) bash --init-file $KDEV_BASEDIR/kdevplatform_shell_environment.sh -i"
    ssh $@ -t -R localhost:$DBUS_FORWARDING_TCP_TARGET_PORT:localhost:$DBUS_FORWARDING_TCP_LOCAL_PORT \
           "APPLICATION=$APPLICATION KDEV_BASEDIR=$KDEV_BASEDIR KDEV_DBUS_ID=$KDEV_DBUS_ID FORWARD_DBUS_FROM_PORT=$DBUS_FORWARDING_TCP_TARGET_PORT APPLICATION_HOST=$APPLICATION_HOST DBUS_SOCKET_SUFFIX=$(getDBusAbstractSocketSuffix) bash --init-file $KDEV_BASEDIR/kdevplatform_shell_environment.sh -i"
    kill %1 # Kill the forwarding loop
}

if [ "$FORWARD_DBUS_FROM_PORT" ]; then
    echo "Initializing DBUS forwarding to host $APPLICATION_HOST"
    export DBUS_SESSION_BUS_ADDRESS=unix:abstract=${DBUS_ABSTRACT_SOCKET_TARGET_BASE_PATH}-${DBUS_ABSTRACT_SOCKET_TARGET_INDEX}${DBUS_SOCKET_SUFFIX}
    keepForwardingDBusFromTCPSocket
fi

##### INITIALIZATION --------------------------------------------------------------------------------------------------------------------

updateShellPrompt
echo "Initialized"

help
