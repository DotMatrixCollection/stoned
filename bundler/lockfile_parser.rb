require "bundler"

module Bundler
  class LockfileParser
    Spec = Struct.new(:name, :version)

    attr_reader :specs, :platforms, :dependencies, :bundler_version

    def initialize(contents)
      @specs = []
      @platforms = []
      @dependencies = []
      @bundler_version = nil
      parse(contents.to_s)
    end

    private

    def parse(contents)
      current_section = nil

      contents.each_line do |line|
        stripped = line.chomp

        if stripped =~ /^(GEM|PATH|GIT|PLATFORMS|DEPENDENCIES|BUNDLED WITH)\s*$/
          current_section = $1
          next
        end

        if (current_section == "GEM" || current_section == "PATH" || current_section == "GIT") &&
           stripped =~ /^\s{4}(\S+)\s+\(([^)]+)\)/
          @specs << Spec.new($1, $2)
          next
        end

        if current_section == "PLATFORMS" && stripped =~ /^\s{2}(\S+)/
          @platforms << $1
          next
        end

        if current_section == "DEPENDENCIES" && stripped =~ /^\s{2}(\S+)/
          @dependencies << $1.sub(/!$/, "")
          next
        end

        if current_section == "BUNDLED WITH" && stripped =~ /^\s+(\S+)/
          @bundler_version = $1
        end
      end
    end
  end
end
