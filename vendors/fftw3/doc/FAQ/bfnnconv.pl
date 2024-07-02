#!/usr/bin/perl --
# Copyright (C) 1993-1995 Ian Jackson.

# This file is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.

# It is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with GNU Emacs; see the file COPYING.  If not, write to
# the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
# Boston, MA 02111-1307, USA.

# (Note: I do not consider works produced using these BFNN processing
# tools to be derivative works of the tools, so they are NOT covered
# by the GPL.  However, I would appreciate it if you credited me if
# appropriate in any documents you format using BFNN.)

@outputs=('ascii','info','html');

while ($ARGV[0] =~ m/^\-/) {
    $_= shift(@ARGV);
    if (m/^-only/) {
        @outputs= (shift(@ARGV));
    } else {
        warn "unknown option `$_' ignored";
    }
}

$prefix= $ARGV[0];
$prefix= 'stdin' unless length($prefix);
$prefix =~ s/\.bfnn$//;

if (open(O,"$prefix.xrefdb")) {
    @xrefdb= <O>;
    close(O);
} else {
    warn "no $prefix.xrefdb ($!)";
}

$section= -1;
for $thisxr (@xrefdb) {
    $_= $thisxr;
    chop;
    if (m/^Q (\w+) ((\d+)\.(\d+)) (.*)$/) {
        $qrefn{$1}= $2;
        $qreft{$1}= $5;
        $qn2ref{$3,$4}= $1;
        $maxsection= $3;
        $maxquestion[$3]= $4;
    } elsif (m/^S (\d+) /) {
        $maxsection= $1;
        $sn2title{$1}=$';
    }
}

open(U,">$prefix.xrefdb-new");

for $x (@outputs) { require("m-$x.pl"); }

&call('init');

while (<>) {
    chop;
    next if m/^\\comment\b/;
    if (!m/\S/) {
        &call('endpara');
        next;
    }
    if (s/^\\section +//) {
        $line= $_;
        $section++; $question=0;
        print U "S $section $line\n";
        $|=1; print "S$section",' 'x10,"\r"; $|=0;
        &call('endpara');
        &call('startmajorheading',"$section",
              "Section $section",
              $section<$maxsection ? "Section ".($section+1) : '',
              $section>1 ? 'Section '.($section-1) : 'Top');
        &text($line);
        &call('endmajorheading');
        if ($section) {
            &call('endpara');
            &call('startindex');
            for $thisxr (@xrefdb) {
                $_= $thisxr;
                chop;
                if (m/^Q (\w+) (\d+)\.(\d+) (.*)$/) {
                    $ref= $1; $num1= $2; $num2= $3; $text= $4;
                    next unless $num1 == $section;
                    &call('startindexitem',$ref,"Q$num1.$num2","Question $num1.$num2");
                    &text($text);
                    &call('endindexitem');
                }
            }
            &call('endindex');
        }
    } elsif (s/^\\question \d{2}[a-z]{3}((:\w+)?) +//) {
        $line= $_;
        $question++;
        $qrefstring= $1;
        $qrefstring= "q_${section}_$question" unless $qrefstring =~ s/^://;
        print U "Q $qrefstring $section.$question $line\n";
        $|=1; print "Q$section.$question",' 'x10,"\r"; $|=0;
        &call('endpara');
        &call('startminorheading',$qrefstring,
              "Question $section.$question",
              $question < $maxquestion[$section] ? "Question $section.".($question+1) :
              $section < $maxsection ? "Question ".($section+1).".1" : '',
              $question > 1 ? "Question $section.".($question-1) :
              $section > 1 ? "Question ".($section-1).'.'.($maxquestion[$section-1]) :
              'Top',
              "Section $section");
        &text("Question $section.$question.  $line");
        &call('endminorheading');
    } elsif (s/^\\only +//) {
        @saveoutputs= @outputs;
        @outputs=();
        for $x (split(/\s+/,$_)) {
            push(@outputs,$x) if grep($x eq $_, @saveoutputs);
        }
    } elsif (s/^\\endonly$//) {
        @outputs= @saveoutputs;
    } elsif (s/^\\copyto +//) {
        $fh= $';
        while(<>) {
            last if m/^\\endcopy$/;
            while (s/^([^\`]*)\`//) {
                print $fh $1;
                m/([^\\])\`/ || warn "`$_'";
                $_= $';
                $cmd= $`.$1;
                $it= `$cmd`; chop $it;
                print $fh $it;
            }
            print $fh $_;
        }
    } elsif (m/\\index$/) {
        &call('startindex');
        for $thisxr (@xrefdb) {
            $_= $thisxr;
            chop;
            if (m/^Q (\w+) (\d+\.\d+) (.*)$/) {
                $ref= $1; $num= $2; $text= $3;
                &call('startindexitem',$ref,"Q$num","Question $num");
                &text($text);
                &call('endindexitem');
            } elsif (m/^S (\d+) (.*)$/) {
                $num= $1; $text= $2;
                next unless $num;
                &call('startindexmainitem',"s_$num",
                      "Section $num.","Section $num");
                &text($text);
                &call('endindexitem');
            } else {
                warn $_;
            }
        }
        &call('endindex');
    } elsif (m/^\\call-(\w+) +(\w+)\s*(.*)$/) {
        $fn= $1.'_'.$2;
        eval { &$fn($3); };
        warn $@ if length($@);
    } elsif (m/^\\call +(\w+)\s*(.*)$/) {
        eval { &call($1,$2); };
        warn $@ if length($@);
    } elsif (s/^\\set +(\w+)\s*//) {
        $svalue= $'; $svari= $1;
        eval("\$user_$svari=\$svalue"); $@ && warn "setting $svalue failed: $@\n";
    } elsif (m/^\\verbatim$/) {
        &call('startverbatim');
        while (<>) {
            chop;
            last if m/^\\endverbatim$/;
            &call('verbatim',$_);
        }
        &call('endverbatim');
    } else {
        s/\.$/\. /;
        &text($_." ");
    }
}

print ' 'x25,"\r";
&call('finish');
rename("$prefix.xrefdb-new","$prefix.xrefdb") || warn "rename xrefdb: $!";
exit 0;


sub text {
    local($in,$rhs,$word,$refn,$reft,$fn,$style);
    $in= "$holdover$_[0]";
    $holdover= '';
    while ($in =~ m/\\/) {
#print STDERR ">$`##$'\n";
        $rhs=$';
        &call('text',$`);
        $_= $rhs;
        if (m/^\w+ $/) {
            $holdover= "\\$&";
            $in= '';
        } elsif (s/^fn\s+([^\s\\]*\w)//) {
            $in= $_;
            $word= $1;
            &call('courier');
            &call('text',$word);
            &call('endcourier');
        } elsif (s/^tab\s+(\d+)\s+//) {
            $in= $_; &call('tab',$1);
        } elsif (s/^nl\s+//) {
            $in= $_; &call('newline');
        } elsif (s/^qref\s+(\w+)//) {
            $refn= $qrefn{$1};
            $reft= $qreft{$1};
            if (!length($refn)) {
                warn "unknown question `$1'";
            }
            $in= "$`\\pageref:$1:$refn:$reft\\endpageref.$_";
        } elsif (s/^pageref:(\w+):([^:\n]+)://) {
            $in= $_;
            &call('pageref',$1,$2);
        } elsif (s/^endpageref\.//) {
            $in= $_; &call('endpageref');
        } elsif (s/^(\w+)\{//) {
            $in= $_; $fn= $1;
            eval { &call("$fn"); };
            if (length($@)) { warn $@; $fn= 'x'; }
            push(@styles,$fn);
        } elsif (s/^\}//) {
            $in= $_;
            $fn= pop(@styles);
            if ($fn ne 'x') { &call("end$fn"); }
        } elsif (s/^\\//) {
            $in= $_;
            &call('text',"\\");
        } elsif (s,^(\w+)\s+([-A-Za-z0-9.\@:/]*\w),,) {
#print STDERR "**$&**$_\n";
            $in= $_;
            $style=$1; $word= $2;
            &call($style);
            &call('text',$word);
            &call("end$style");
        } else {
            warn "unknown control `\\$_'";
            $in= $_;
        }
    }
    &call('text',$in);
}


sub call {
    local ($fnbase, @callargs) = @_;
    local ($coutput);
    for $coutput (@outputs) {
        if ($fnbase eq 'text' && eval("\@${coutput}_cmds")) {
#print STDERR "special handling text (@callargs) for $coutput\n";
            $evstrg= "\$${coutput}_args[\$#${coutput}_args].=\"\@callargs\"";
            eval($evstrg);
            length($@) && warn "call adding for $coutput (($evstrg)): $@";
        } else {
            $fntc= $coutput.'_'.$fnbase; 
            &$fntc(@callargs);
        }
    }
}


sub recurse {
    local (@outputs) = $coutput;
    local ($holdover);
    &text($_[0]);
}


sub arg {
#print STDERR "arg($_[0]) from $coutput\n";
    $cmd= $_[0];
    eval("push(\@${coutput}_cmds,\$cmd); push(\@${coutput}_args,'')");
    length($@) && warn "arg setting up for $coutput: $@";
}

sub endarg {
#print STDERR "endarg($_[0]) from $coutput\n";
    $evstrg= "\$${coutput}_cmd= \$cmd= pop(\@${coutput}_cmds); ".
             "\$${coutput}_arg= \$arg= pop(\@${coutput}_args); ";
    eval($evstrg);
    length($@) && warn "endarg extracting for $coutput (($evstrg)): $@";
#print STDERR ">call $coutput $cmd $arg< (($evstrg))\n";
    $evstrg= "&${coutput}_do_${cmd}(\$arg)";
    eval($evstrg);
    length($@) && warn "endarg running ${coutput}_do_${cmd} (($evstrg)): $@";
}
