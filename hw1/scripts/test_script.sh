mkdir -p dump

port=8888
if [ "$#" -eq 1 ]; then
    port=$1
fi

echo "troll face test..."
echo "-----------------------------"
curl localhost:${port}/trollface.txt -i -H "Range: bytes=0-2000" # (206)
curl localhost:${port}/trollface.txt -i -H "Range: bytes=500-2000" # test variable range out of bounds (206)
curl localhost:${port}/trollface.txt -i -H "Range: bytes=2000-3000" # test invalid response (416)
curl localhost:${port}/trollface.txt -i  # take the entire file (200)
echo

echo "png test..."
echo "-----------------------------"
curl localhost:${port}/image/aaa_comp.png --head # show headers of response
curl localhost:${port}/image/aaa_comp.png --output dump/aaa_comp_1.png
curl localhost:${port}/image/aaa.png -H "Range: bytes=0-5000'" --output dump/test.png # test variable range on png
