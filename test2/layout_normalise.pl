#!/usr/bin/perl
use warnings;

sub emit;

sub is_zero
{
	return shift =~ /^(space|zero)$/;
}

my %sizes = (
	'byte'  => 1,
	'word'  => 2,
	'long'  => 4,
	'quad'  => 8,
);
my %sizes_r = map { $sizes{$_} => $_ } keys %sizes;

my @parts; # { size, value } or { lbl }

die "Usage: $0 fname\n" unless @ARGV == 1;
my $fnam = shift;
open F, '<', $fnam or die "open $fnam: $!\n";
while(<F>){
	if(/^[ \t]*\.(byte|word|long|quad|zero|space)[ \t]+(.*)/){
		my $is_zero = is_zero($1);

		for(split /, */, $2){
			my $r;
			if($is_zero){
				$r = { size  => $_,
					     value => 0 };
			}else{
				$r = { size  => $sizes{$1},
					     value => $_ };
			}

			emit($r);
		}
	}elsif(/(.*): *$/){
		emit({ lbl => $1 });
	}
}
close F;

sub flush;
sub emit2;
my $last;
END {
	flush();
}

sub flush
{
	if($last){
		emit2 $last;
		$last = undef;
	}
}

sub emit2
{
	my $m = shift;
	if($m->{lbl}){
		print "$m->{lbl}:$/";
	}elsif($m->{value}){
		print ".$sizes_r{$m->{size}} $m->{value}\n";
	}else{
		print ".zero $m->{size}\n";
	}
}

sub emit
{
	my $m = shift;
	if($m->{lbl}){
		;
	}elsif($m->{value} =~ /^[0-9]+$/ and $m->{value} == 0){
		if($last){
			$last->{size} += $m->{size};
		}else{
			$last = $m;
		}
		return;
	}
	flush();
	emit2($m);
}
