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

function print_usage {
  echo ""
  echo "Use -h or --help to show this help."
  echo "Usage:"
  echo "$0 [-d|--download] [-g|--generate] [-r|--run (-l|--labels <number of labels>) [benchmark|test]|[<path to graph file>]]"
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
    *)
    echo "Unkwown option '$1'. See usage:"
    print_usage
    exit 0
    # unknown_args+=("$1") # save it in an array for later
    # shift # past argument
    ;;
  esac
done