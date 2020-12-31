#!/bin/bash

DATA_PATH="data"
GRAIL_DATA_PATH="$DATA_PATH/grail-dags"
GEN_SCRIPT_PATH="graph-generator-stq"
GEN_DATA_PATH="$DATA_PATH/gen-dags"
URL_PREFIX="https://storage.googleapis.com/google-code-archive-downloads/v2/code.google.com/grail"
DAGS=(
  "updates_frwiki_graph.dat"
  "init_frwiki_graph.dat"
  "testdata.tar.gz"
  "scaleFree.tar.gz"
  "sigmod09.tar.gz"
  "sigmod08.tar.gz"
  "uniprotenc_150m.scc.gra.gz"
  "uniprotenc_100m.scc.gra.gz"
  "uniprotenc_22m.scc.gra.gz"
  "go_uniprot.gra.gz"
  "citeseer.scc.gra.gz"
  "cit-Patents.scc.gra.gz"
  "citeseerx.gra.gz"
)
LABELS=5
RUN_MODE="all"  # benchmark -> DAGs from $GRAIL_DATA_PATH | test -> DAGs from $GEN_DATA_PATH | all -> both | specific -> only given graph path
SPECIFIC_DAG_PATH=""

function print_usage {
  echo ""
  echo "Use -h or --help to show this help."
  echo "Usage:"
  echo "$0 [-d|--download] [-g|--generate] [-r|--run (-l|--labels <number of labels>) [benchmark|test|<path to graph file>]]"
  echo "[] mean an argument is optional"
  echo "() mean an argument is mandatory"
  echo "| means it is possible to choose the argument on its left or on its right"
  echo ""
  echo "You need the following packages to run this script:"
  echo "wget, gzip, tar, make, gcc"
  echo ""
}

function download_graphs {
  # if the dir does not exist
  if [[ ! -d "$GRAIL_DATA_PATH" ]]; then
    mkdir -p "$GRAIL_DATA_PATH"
  # the dir exists but is not empty
  elif [[ -n "$(ls -A "$GRAIL_DATA_PATH")" ]]; then
    echo "It seems you have already downloaded the DAGs from the Internet."
    echo "Please check if everything is fine in $GRAIL_DATA_PATH"
    echo "Skipping download..."
    return
  fi
  echo "You are about to download ~386MB from the Internet and then extract them to ~1.3GB on disk."
  echo "Do you want to proceed? [y/N]"
  read -r ans
  if [ "$ans" = "y" ] || [ "$ans" = "Y" ]; then
    echo "Downloading DAGs from Google Code Archive..."
    for dag in "${DAGS[@]}"; do
      wget "$URL_PREFIX"/"$dag" --output-document "$GRAIL_DATA_PATH"/"$dag"
    done
  else
    echo "Skipping download..."
  fi
}

function extract_downloaded_graphs {
  echo "Extracting downloaded DAGs..."
  for dag in "$GRAIL_DATA_PATH"/*.gz; do
    [[ -e "$dag" ]] || break  # there are no .gz files
    echo "Decompressing $dag with gzip..."
    gzip --decompress "$dag"
  done
  for dag in "$GRAIL_DATA_PATH"/*.tar; do
    [[ -e "$dag" ]] || break  # there are no .tar files
    echo "Unarchiving $dag with tar..."
    tar -xf "$dag" --directory "$GRAIL_DATA_PATH"
    rm "$dag"
  done
  # move .gra files from .tar to parent directory
  # run twice since the maximum number of nested directories is 2
  for _ in {1..2}; do
    for dir in "$GRAIL_DATA_PATH"/*; do
      if [[ -d "$dir" ]]; then
        mv "$dir"/* "$GRAIL_DATA_PATH"
        rmdir "$dir"
      fi
    done
  done
  # TODO: how to treat .test files? do we only consider some of them for queries?
  for query_file in "$GRAIL_DATA_PATH"/*.test; do
    [[ -e "$query_file" ]] || break  # there is no .test file
    # change extension: .test -> .que
    output_file="${query_file%%.*}.que"
    mv "$query_file" "$output_file"
    echo "Renamed $query_file to $output_file"
  done
  CLEAN_EXE="clean_query_file"
  echo "Compiling $CLEAN_EXE.c..."
  gcc -Wall $CLEAN_EXE.c -o $CLEAN_EXE
  for query_file in "$GEN_DATA_PATH"/*.que; do
    [[ -e "$query_file" ]] || break  # there is no .que file
    [[ -z $(head -1 "$query_file" | cut -d ' ' -f 3) ]] && break  # there is no third number on the first line, skip file
    echo "Removing excess number field from $query_file..."
    ./$CLEAN_EXE "$query_file"
  done
  for query_file in "$GRAIL_DATA_PATH"/*.que; do
    [[ -e "$query_file" ]] || break  # there is no .que file
    [[ -z $(head -1 "$query_file" | cut -d ' ' -f 3) ]] && break  # there is no third number on the first line, skip file
    echo "Removing excess number field from $query_file..."
    ./$CLEAN_EXE "$query_file"
  done
}

function generate_graphs {
  graphs=($(find "$GEN_SCRIPT_PATH" -maxdepth 1 -name "*.gra"))
  # if there are no .gra files in the script directory
  if [[ ${#graphs[@]} -eq 0 ]]; then
    # see cd handling: https://github.com/koalaman/shellcheck/wiki/SC2103
    cd "$GEN_SCRIPT_PATH" || exit
    make
    echo "Running $GEN_SCRIPT_PATH/run.sh, which contains the following calls:"
    cat run.sh
    ./run.sh
    cd ..
  else
    echo "It seems you have already generated the benchmark graphs."
    echo "Skipping DAG generation..."
  fi
  if [[ ! -d "$GEN_DATA_PATH" ]]; then
    mkdir -p "$GEN_DATA_PATH"
  fi
  echo "Moving generated DAGs to $GEN_DATA_PATH..."
  for dag in "$GEN_SCRIPT_PATH"/*.gra; do
    echo -n "$dag" | cut -d / -f 2
  done
  mv "$GEN_SCRIPT_PATH"/*.gra "$GEN_DATA_PATH"
  mv "$GEN_SCRIPT_PATH"/*.que "$GEN_DATA_PATH"
}

function run_cmd {
  cmd="$EXE $1.gra $LABELS $1.que"
  echo ""
  echo "Running $cmd"
  $cmd
}

function run_benchmark {
  number_regex='^[0-9]+$'
  graph_regex='^.+.gra$'
  # parse arguments after -r|--run
  while [[ $# -gt 0 ]]; do
    arg="$1"
    case $arg in
      -l|--labels)
      if [[ $2 =~ $number_regex ]]; then
        echo "Set number of labels to $2"
        LABELS=$2
      else
        echo "Unexpected number of labels: $2, using default $LABELS..."
      fi
      shift 2
      ;;
      benchmark|test)
      echo "Run mode set to $arg only"
      RUN_MODE=$arg
      shift
      ;;
      *)  # assuming path to specific graph
      # remove file extension (.gra) - see https://stackoverflow.com/a/61294531
      base_name=${arg%%.*}
      if [[ -f "$arg" ]] && [[ -f "$base_name".que ]] && [[ "$arg" =~ $graph_regex ]]; then
        if [[ "$RUN_MODE" = "benchmark" ]] ||  [[ "$RUN_MODE" = "test" ]]; then
          echo "Conflicting run mode: benchmark or test is set, but a DAG path has also been given. Please see the usage:"
          print_usage
          exit 1
        fi
        echo "Run mode set to specific. DAG path recognized: $arg"
        RUN_MODE="specific"
        SPECIFIC_DAG_PATH="$base_name"
        shift
      else
        echo "Unexpected DAG (.gra file): $arg. Terminating..."
        exit 1
      fi
      ;;
    esac
  done

  # compile our program
  make
  # make clean
  # the name of our executable
  EXE="./$(grep ^EXECUTABLE Makefile | cut -d ' ' -f 3)"

  if [[ "$RUN_MODE" = "all" ]]; then
    echo "Run mode set to all: running $EXE against all benchmark and test DAGs"
  fi

  echo "Number of labels: $LABELS"
  if [[ "$RUN_MODE" = "specific" ]]; then
      run_cmd "$SPECIFIC_DAG_PATH"
  else
      if [[ "$RUN_MODE" = "benchmark" ]] || [[ "$RUN_MODE" = "all" ]]; then
        for graph_file in "$GRAIL_DATA_PATH"/*.gra; do
          base_name=${graph_file%%.*}
          run_cmd "$base_name"
        done
      fi
      if [[ "$RUN_MODE" = "test" ]] || [[ "$RUN_MODE" = "all" ]]; then
        for graph_file in "$GEN_DATA_PATH"/*.gra; do
          base_name=${graph_file%%.*}
          run_cmd "$base_name"
        done
      fi
  fi
  exit 0
}

# no arguments given
if [[ $# -eq 0 ]]; then
  print_usage
  exit 0
fi

# parse arguments
# unknown_args=()
while [[ $# -gt 0 ]]; do
  arg="$1"
  case $arg in
    -h|--help)
    print_usage
    exit 0
    ;;
    -d|--download)
    download_graphs
    extract_downloaded_graphs
    shift
    ;;
    -g|--generate)
    generate_graphs
    shift
    ;;
    -r|--run)
    # the $@ are all the remaining arguments (e.g. -l 2, but not -h or -d)
    shift
    run_benchmark "$@"
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