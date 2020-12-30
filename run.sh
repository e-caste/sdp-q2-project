#!/bin/bash

function print_usage {
  echo ""
  echo "Use -h or --help to show this help."
  echo "Usage:"
  echo "$0 [-d|--download] [-g|--generate] [-r|--run (-l|--labels <number of labels>) [benchmark|test]|[<path to graph file>]]"
  echo "[] mean an argument is optional"
  echo "() mean an argument is mandatory"
  echo "| means it is possible to choose the argument on its left or on its right"
  echo ""
}

# parse arguments
# unknown_args=()
while [[ $# -ge 0 ]]; do
  arg="$1"
  case $arg in
    -h|--help)
    print_usage
    exit 0
    ;;

    *)
    echo "Unkwown option '$1'. See usage:"
    print_usage
    exit 0
    # unknown_args+=("$1") # save it in an array for later
    # shift # past argument
    ;;
  esac
done