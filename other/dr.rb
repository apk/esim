puts '#** Generated **#

1u begin =1+3u
2u 800m
<hp "1"
3u >w / sw "a" 20d
1u 800m
1u 800m
<hp "722"
1u 200m
1u 800m
<hp "721"
1u 200m
1u 800m
3u >w / sw "a" 20d
1u 800m
end "71c72"

1u begin +2u "85c86"
w< / se "b" 20d
1u 800m
1u 800m
<hp "86A"
1u 200m
end "b"
'

def line a, b, c, d
  puts '
1v begin "$1c$2" =3+4u
3v 800m
<hp "$2A"
3v 200m
w< 3u / se "$2a" 10d
hs> "$2P1"
7v 800m
<hs "$2N1"
3u >w / sw "$2b" 10d
3v 200m
hp> "$2F"
1u 800m
1u 800m
<hp "$31"
1u 200m
hp> "$32"
1u 800m
1u 800m
<hp "$3A"
3v 200m
w< 3u / se "$3a" 10d
hs> "$3P1"
7v 800m
<hs "$3N1"
3u >w / sw "$3b" 10d
3v 200m
hp> "$3F"
1u 800m
1u 800m
<hp "$41"
1u 200m
hp> "$42"
3v 800m
end "$3c$4"

6u begin +1u "$2a" se
~ 50m e
hs> "$2P2"
7v 800m
<hs "$2N2"
~ 50m ne
end "$2b"

57v begin +0u "$3a" se
~ 50m e
hs> "$3P2"
7v 800m
<hs "$3N2"
~ 50m ne
end "$3b"

%def $2A-$2N1/$2N2
%def $2N1-$31
%def $2N2-$31
%def $2F-$2P1/$2P2
%def $2P1-$22
%def $2P2-$22

%def $3A-$3N1/$3N2
%def $3N1-$41
%def $3N2-$41
%def $3F-$3P1/$3P2
%def $3P1-$32
%def $3P2-$32

%quo1 $21 $2A $2N1 $2N2
%quo1 $32 $2F $2P1 $2P2
%quo1 $31 $3A $3N1 $3N2
%quo1 $42 $3F $3P1 $3P2

'.gsub(/\$1/,a).gsub(/\$2/,b).gsub(/\$3/,c).gsub(/\$4/,d)

end

line '71', '72', '73', '74'
line '73', '74', '75', '76'
line '75', '76', '77', '78'

puts '
1v begin "77c78" =5+7u
3v 800m
<hp "78A"
3v 200m
w< / se "78c" 10d
w< 3u / ne "78a" 10d
hs> "78P1"
7v 800m
<hs "78N1"
3u >w / nw "78b" 10d
>w / sw "78e" 10d
3v 200m
hp> "78H"
1u 800m
1u 800m
<hp "78I"
3v 200m
w< 3u / se "78d" 13d
3v 200m
hp> "78F"
1u 800m
end "78c79"

8u begin -1u "78a" ne
~ 50m e
hs> "78P2"
7v 800m
<hs "78N2"
~ 50m se
end "78b"

6u begin +2u "78c" se
w< / 3u 75m e "78f" 10d
~ 50m e
hs> "78P3"
7v 800m
<hs "78N3"
~ 50m ne
>w / 3u 75m w "78g" 10d
end "78e"

10u begin +1u "78f"
hs> "78P4"
7v 800m
<hs "78N4"
end "78g"

57v begin -1u "78d" se
~ 50m e
3v 200m
hp> "78G" 13d
1u 400m
end "78c90"

9u begin "78c79" =4+10u
1u 800m
<hp "79A"
3v 200m
3u >w / sw "79c" 13d
3v 200m
hp> "79H"
1u 800m
1u 800m
<hp "79I"
3v 200m
w< / se "79e" 10d
w< 3u / ne "79a" 10d
hs> "79P1"
7v 800m
<hs "79N1"
3u >w / nw "79b" 10d
>w / sw "79d" 10d
3v 200m
hp> "79F"
1u 800m
1u 800m
<hp "801"
1u 200m
hp> "802"
3v 800m
end "79c80"

9u begin +2u "79c90"
1u 400m
<hp "79B" 13d
3v 200m
~ ne 50m
end "79c"

26u begin -3u "79a" ne
~ 50m e
hs> "79P2"
7v 800m
<hs "79N2"
~ 50m se
end "79b"

24u begin +2u "79e" se
w< / 3u e "79f" 10d
~ e 50m
hs> "79P3"
7v 800m
<hs "79N3"
~ 50m ne
>w / 3u w "79g" 10d
end "79d"

28u begin +1u "79f" e
hs> "79P4"
7v 800m
<hs "79N4"
end "79g"

%def 78A-78N1/78N2/78N3/78N4
%def 78N1-78I
%def 78N2-78I
%def 78N3-78I
%def 78N4-78I
%def 78I-79A
%mac b 78I-90F
%def 78F-78H
%def 78G-78H
%def 78H-78P1/78P3/78P2/78P4
%def 78P1-782
%def 78P2-782
%def 78P3-782
%def 78P4-782

%def 79A-79I
%def 79B-79I
%def 79I-79N1/79N2/79N3/79N4
%def 79N1-801
%def 79N2-801
%def 79N3-801
%def 79N4-801
%def 79F-79P1/79P2/79P3/79P4
%def 79P1-79H
%def 79P2-79H
%def 79P3-79H
%def 79P4-79H
%def 79H-78F
%mac c 79H-90G

%quo1 79A 79H
%quo1 79B 79H

%quo1 78I 78F
%quo1 78I 78G

%quo1 78I 79H

%quo3 781 78A 78N1 78N2 78N3 78N4
%quo3 79H 78F 78G 78H 78P1 78P2 78P3 78P4
%quo4 781 78A 78N1 78N2 78N3 78N4 79H 78F 78G 78H 78P1 78P2 78P3 78P4

%quo3 78I 79A 79B 79I 79N1 79N2 79N3 79N4
%quo3 802 79F 79P1 79P2 79P3 79P4
%quo4 78I 79A 79B 79I 79N1 79N2 79N3 79N4 802 79F 79P1 79P2 79P3 79P4
'

puts '
1v begin "69c90" =3+6u
3v 800m
<hp "90A"
3v 200m
w< / se "90a" 10d
w< 3u / ne "90b" 10d
hs> "90P1"
3v 400m
w< ne 13d / se "90c" 13d
3v 400m e
<hs "90N1"
3u >w / nw "90d" 10d
3v 200m
hp> "90F" 13d
1u 400m
end "78c90"

14u begin +1u "90c"
3v 400m
<hs "90N4"
3u >w / sw "90e" 10d
3v 200m
hp> "90G" 13d
1u 400m
end "79c90"

8u begin -2u "90b" ne
2u ~ 75m e
hs> "90P2"
4u 825m
<hs "90N2"
~ 50m se
end "90d"

6u begin +2u "90a" se
2u ~ 3u 125m e
hs> "90P3"
4u 825m
<hs "90N3"
~ 50m ne
end "90e"

%def 90A-90N1/90N2
%def 90N1-78G
%def 90N2-78G
%mac c 90A-90N4/90N3
%mac c 90N3-79B
%mac c 90N4-79B
%def 90F-90P1/90P2
%def 90G-90P1/90P3
%def 90P1-902
%def 90P2-902
%def 90P3-902
'

line '79', '80', '81', '82'
line '81', '82', '83', '84'
line '83', '84', '85', '86'

puts '
1u begin =3+3u "c"
1u 800m
1u 800m
<hp "642"
1u 200m
1u 800m
<hp "641"
1u 200m
1u 800m
3u >w / sw "c" 20d
1u 800m
end "63c64"
'

line '63', '64', '65', '66'
line '65', '66', '67', '68'
line '67', '68', '69', '90'

puts '%quo1 861 86A'

[1,2,3,4,5,6].each do |x|
  puts '%trn len=310 v=200 n=a '
  puts '%trn len=310 v=200 n=b %b'
  puts '%trn len=310 v=200 n=c %c'
end

puts '
%quo1 901 90A 90N1 90N2 90N3 90N4
%quo1 78I 90F 90P1 90P2
%quo1 79H 90G 90P1 90P3
'
