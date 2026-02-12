script_folder="/home/nickzt/Projects/TactOrder/MNNLLama/inference-services/mnn-service"
echo "echo Restoring environment" > "$script_folder/deactivate_conanrunenv-release-x86_64.sh"
for v in GRPC_DEFAULT_SSL_ROOTS_FILE_PATH OPENSSL_MODULES
do
   is_defined="true"
   value=$(printenv $v) || is_defined="" || true
   if [ -n "$value" ] || [ -n "$is_defined" ]
   then
       echo export "$v='$value'" >> "$script_folder/deactivate_conanrunenv-release-x86_64.sh"
   else
       echo unset $v >> "$script_folder/deactivate_conanrunenv-release-x86_64.sh"
   fi
done

export GRPC_DEFAULT_SSL_ROOTS_FILE_PATH="/home/nickzt/.conan2/p/grpc0b2b374459c33/p/res/grpc/roots.pem"
export OPENSSL_MODULES="/home/nickzt/.conan2/p/opensbaa576ceaa566/p/lib/ossl-modules"