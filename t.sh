
  B=http://julius-box.local; TAG=04e18772d42a81
  mapfile -t FS < <(curl -s "$B/peer-album?tag=$TAG" | awk -F'\t' 'NR>1&&NR<4{print $2}')

  # Basiswert, eine Verbindung
  curl -s -o /dev/null -w 'single: %{speed_download} B/s  %{size_download} B  exit=%{exitcode}\n' \
       "$B/peer-file?tag=$TAG&name=${FS[0]}"

  # zwei Verbindungen, mit Gesamtzeit
  t0=$(date +%s.%N)
  for f in "${FS[@]}"; do
    curl -s -o /dev/null -w "par:    %{speed_download} B/s  %{size_download} B  exit=%{exitcode}  $f\n" \
         "$B/peer-file?tag=$TAG&name=$f" &
  done; wait
  t1=$(date +%s.%N); echo "Gesamtzeit: $(echo "$t1-$t0" | bc) s"
