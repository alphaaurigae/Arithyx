#!/usr/bin/env perl

use strict;
use warnings;
use Math::BigFloat;
use Math::BigInt;

$| = 1;

my $HUNTER = './build/bin/math-hunter';
my $VERIFIER = './build/bin/math-verifier';

my $BEGIN = '20.121111111111111111';
my $END = '20.121111111111111112';
my $STEP = '0.000000000000000001';

my $MAX_CHECKPOINTS = 1_000_000;

my $OUT_DIR = './build/bin/benchmarks/kurepa';

sub fail {
    my ($message) = @_;
    print STDERR "error: $message\n";
    exit 1;
}

-f $HUNTER && -x $HUNTER
    or fail("$HUNTER not found or not executable");

-f $VERIFIER && -x $VERIFIER
    or fail("$VERIFIER not found or not executable");

mkdir './build/bin/benchmarks'
    unless -d './build/bin/benchmarks';

mkdir $OUT_DIR
    unless -d $OUT_DIR;

sub decimal_parts {
    my ($value) = @_;

    $value =~ /\A(\d+)(?:\.(\d+))?\z/
        or fail("invalid decimal value: $value");

    my $integer = $1;
    my $fraction = defined $2 ? $2 : '';

    return ($integer, $fraction);
}

my $scale = 0;

for my $value ($BEGIN, $END, $STEP) {
    my (undef, $fraction) = decimal_parts($value);

    $scale = length($fraction)
        if length($fraction) > $scale;
}

my $scale_factor = Math::BigInt->new(10)->bpow($scale);

sub decimal_to_fixed {
    my ($value) = @_;

    my ($integer, $fraction) = decimal_parts($value);

    $fraction .= '0' x ($scale - length($fraction));

    my $fixed = Math::BigInt->new($integer)->bmul($scale_factor);

    $fixed->badd(Math::BigInt->new($fraction))
        if length($fraction);

    return $fixed;
}

sub fixed_to_decimal {
    my ($value) = @_;

    my $integer = $value->copy->bdiv($scale_factor);

    return $integer->bstr
        if $scale == 0;

    my $fraction = $value->copy->bmod($scale_factor)->bstr;

    $fraction = ('0' x ($scale - length($fraction))) . $fraction;

    $fraction =~ s/0+\z//;

    return $integer->bstr
        if $fraction eq '';

    return $integer->bstr . '.' . $fraction;
}

sub exponent_to_integer {
    my ($exponent) = @_;

    my $precision = 100;

    my $power = Math::BigFloat->new($exponent);
    my $ten = Math::BigFloat->new(10);

    $power->accuracy($precision);
    $ten->accuracy($precision);

    my $value = $ten->bpow($power);

    return Math::BigInt->new($value->bfloor->bstr);
}

sub run_command {
    my (@command) = @_;

    print "\n";
    print "Command: ", join(' ', @command), "\n\n";

    system @command;

    if ($? == -1) {
        fail("failed to execute $command[0]: $!");
    }

    if ($? & 127) {
        fail(
            "$command[0] terminated by signal " .
            ($? & 127)
        );
    }

    return $? >> 8;
}

my $begin_fixed = decimal_to_fixed($BEGIN);
my $end_fixed = decimal_to_fixed($END);
my $step_fixed = decimal_to_fixed($STEP);

$step_fixed->is_positive
    or fail('STEP must be greater than zero');

$end_fixed >= $begin_fixed
    or fail('END must be greater than or equal to BEGIN');

my $range_fixed =
    $end_fixed->copy->bsub($begin_fixed);

my $checkpoint_count =
    $range_fixed->copy->bdiv($step_fixed);

if ($range_fixed->copy->bmod($step_fixed)->is_positive) {
    $checkpoint_count->badd(1);
}

$checkpoint_count <= $MAX_CHECKPOINTS
    or fail(
        'configuration requires ' .
        $checkpoint_count->bstr .
        " checkpoints; maximum is $MAX_CHECKPOINTS"
    );

print "========================================\n";
print "Kurepa runner\n";
print "Exponent range: $BEGIN -> $END\n";
print "Exponent step:  $STEP\n";
print "Checkpoints:    ", $checkpoint_count->bstr, "\n";
print "========================================\n";

my $current_fixed = $begin_fixed->copy;
my $checkpoint_index = 0;

while ($current_fixed < $end_fixed) {
    my $next_fixed =
        $current_fixed->copy->badd($step_fixed);

    $next_fixed = $end_fixed->copy
        if $next_fixed > $end_fixed;

    my $current_exponent =
        fixed_to_decimal($current_fixed);

    my $next_exponent =
        fixed_to_decimal($next_fixed);

    my $begin_value =
        exponent_to_integer($current_exponent);

    my $end_value =
        exponent_to_integer($next_exponent);

    $end_value > $begin_value
        or fail(
            "exponent interval $current_exponent -> $next_exponent " .
            "produced no integer progress: " .
            $begin_value->bstr . " -> " .
            $end_value->bstr
        );

    my $range_size =
        $end_value->copy->bsub($begin_value);

    my $range_name =
        "10^${current_exponent}-10^${next_exponent}";

    my $output =
        "$OUT_DIR/$range_name.mhout";

    $checkpoint_index++;

    print "\n";
    print "========================================\n";
    print "Checkpoint: $checkpoint_index/", $checkpoint_count->bstr, "\n";
    print "Kurepa: 10^$current_exponent -> 10^$next_exponent\n";
    print "Range:  $begin_value -> $end_value\n";
    print "Size:   ", $range_size->bstr, " integers\n";
    print "========================================\n";

    my $hunter_status = run_command(
        $HUNTER,
        'search',
        'kurepa',
        '--begin',
        $begin_value->bstr,
        '--end',
        $end_value->bstr,
        '--out',
        $output
    );

    if ($hunter_status != 0) {
        print STDERR "\n";
        print STDERR
            "hunter FAILED at " .
            "10^$current_exponent -> 10^$next_exponent\n";
        exit $hunter_status;
    }

    -s $output
        or fail("hunter produced no output: $output");

    print "\n";
    print "----------------------------------------\n";
    print "Verifier: 10^$current_exponent -> 10^$next_exponent\n";
    print "----------------------------------------\n";

    my $verifier_status = run_command(
        $VERIFIER,
        'verify',
        $output
    );

    if ($verifier_status != 0) {
        print STDERR "\n";
        print STDERR
            "verifier FAILED at " .
            "10^$current_exponent -> 10^$next_exponent\n";
        exit $verifier_status;
    }

    print "\n";
    print "Checkpoint verified successfully.\n";

    $current_fixed = $next_fixed;
}

print "\n";
print "========================================\n";
print "Runner complete\n";
print "Range: 10^$BEGIN -> 10^$END\n";
print "Checkpoints: $checkpoint_index\n";
print "========================================\n";
